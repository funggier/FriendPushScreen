#define WIN32_LEAN_AND_MEAN
#include "network/peer_identity.h"
#include <Windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace fps::network {
namespace {
constexpr DWORD kEncoding=X509_ASN_ENCODING|PKCS_7_ASN_ENCODING;
struct LocalMemory { void* p{}; ~LocalMemory(){if(p)LocalFree(p);} };
struct CngHandle { NCRYPT_HANDLE h{};~CngHandle(){if(h)NCryptFreeObject(h);} };
struct CertCloser { void operator()(const CERT_CONTEXT* p)const {if(p)CertFreeCertificateContext(p);} };
using Certificate=std::unique_ptr<const CERT_CONTEXT,CertCloser>;
struct Store {HCERTSTORE h{};~Store(){if(h)CertCloseStore(h,0);} };
struct ProtectedBlob {
    DATA_BLOB blob{};
    ~ProtectedBlob(){if(blob.pbData){SecureZeroMemory(blob.pbData,blob.cbData);LocalFree(blob.pbData);}}
};
bool Fail(std::string& error,const char* message,DWORD code=GetLastError()) {
    std::ostringstream out;out<<message<<" (0x"<<std::hex<<code<<')';error=out.str();return false;
}
bool ReadBytes(const std::filesystem::path& path,std::vector<std::uint8_t>& bytes,std::string& error) {
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec || size==0 || size>kMaxPairingBytes)return Fail(error,"Missing, empty or oversized identity file",ERROR_INVALID_DATA);
    std::ifstream f(path,std::ios::binary);
    bytes.resize(static_cast<std::size_t>(size));
    if(!f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        return Fail(error,"Identity file read failed",ERROR_READ_FAULT);
    return true;
}
bool WriteAtomic(const std::filesystem::path& path,std::span<const std::uint8_t> bytes,bool replace,std::string& error) {
    if(bytes.empty() || bytes.size()>kMaxPairingBytes)return Fail(error,"Invalid identity file length",ERROR_INVALID_DATA);
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    if(ec)return Fail(error,"Identity directory creation failed",static_cast<DWORD>(ec.value()));
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return Fail(error,"Current identity access failed");
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<std::uint8_t> user(size);
    const bool got=GetTokenInformation(token,TokenUser,user.data(),size,&size)!=FALSE;CloseHandle(token);
    if(!got)return Fail(error,"Current identity SID failed");
    LocalMemory sid;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,reinterpret_cast<LPWSTR*>(&sid.p)))
        return Fail(error,"Identity SID format failed");
    const auto sddl=std::wstring{L"D:P(A;;FA;;;SY)(A;;FA;;;"}+static_cast<wchar_t*>(sid.p)+L")";
    LocalMemory descriptor;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor.p,nullptr))
        return Fail(error,"Identity file protection failed");
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),descriptor.p,FALSE};
    const auto temporary=path.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,&attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return Fail(error,"Identity temporary file creation failed");
    DWORD written=0;const DWORD count=static_cast<DWORD>(bytes.size());
    const bool ok=WriteFile(file,bytes.data(),count,&written,nullptr)!=FALSE && written==count && FlushFileBuffers(file)!=FALSE;
    const auto writeError=GetLastError();CloseHandle(file);
    if(!ok){DeleteFileW(temporary.c_str());return Fail(error,"Identity file write failed",writeError);}
    const DWORD flags=MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0);
    if(!MoveFileExW(temporary.c_str(),path.c_str(),flags)){
        const auto moveError=GetLastError();DeleteFileW(temporary.c_str());return Fail(error,"Identity atomic save failed",moveError);
    }
    return true;
}
bool EncodeObject(LPCSTR type,const void* object,std::vector<std::uint8_t>& bytes) {
    DWORD count=0;
    if(!CryptEncodeObjectEx(X509_ASN_ENCODING,type,object,0,nullptr,nullptr,&count) || count>kMaxPairingBytes)return false;
    bytes.resize(count);
    return CryptEncodeObjectEx(X509_ASN_ENCODING,type,object,0,nullptr,bytes.data(),&count)!=FALSE;
}
std::optional<PublicPeerIdentity> PublicFromCertificate(PCCERT_CONTEXT cert,std::string& error) {
    if(!cert || CertVerifyTimeValidity(nullptr,cert->pCertInfo)!=0) {
        Fail(error,"Certificate is expired or not yet valid",static_cast<DWORD>(CERT_E_EXPIRED));return std::nullopt;
    }
    if(!CryptVerifyCertificateSignatureEx(0,kEncoding,CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
        const_cast<CERT_CONTEXT*>(cert),CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
        const_cast<CERT_CONTEXT*>(cert),0,nullptr)) {
        Fail(error,"Identity certificate must be self-signed");return std::nullopt;
    }
    const auto* algorithm=cert->pCertInfo->SignatureAlgorithm.pszObjId;
    if(!algorithm || std::strcmp(algorithm,szOID_RSA_SHA256RSA)!=0 ||
        CertGetPublicKeyLength(X509_ASN_ENCODING,&cert->pCertInfo->SubjectPublicKeyInfo)<2048) {
        Fail(error,"Unsupported identity signature or key",ERROR_INVALID_DATA);return std::nullopt;
    }
    auto* san=CertFindExtension(szOID_SUBJECT_ALT_NAME2,cert->pCertInfo->cExtension,cert->pCertInfo->rgExtension);
    LocalMemory decoded;DWORD size=0;
    if(!san || !CryptDecodeObjectEx(X509_ASN_ENCODING,X509_ALTERNATE_NAME,san->Value.pbData,san->Value.cbData,
        CRYPT_DECODE_ALLOC_FLAG,nullptr,&decoded.p,&size)) {
        Fail(error,"Identity has no valid DNS SAN");return std::nullopt;
    }
    const auto* names=static_cast<CERT_ALT_NAME_INFO*>(decoded.p);
    if(names->cAltEntry!=1 || names->rgAltEntry[0].dwAltNameChoice!=CERT_ALT_NAME_DNS_NAME ||
        !names->rgAltEntry[0].pwszDNSName) {
        Fail(error,"Unsupported identity SAN",ERROR_INVALID_DATA);return std::nullopt;
    }
    const std::wstring wideName=names->rgAltEntry[0].pwszDNSName;
    if(wideName.size()!=44 || !wideName.starts_with(L"fps-") || !wideName.ends_with(L".invalid") ||
        !std::all_of(wideName.begin()+4,wideName.begin()+36,[](wchar_t c){return (c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})) {
        Fail(error,"Invalid stable peer name",ERROR_INVALID_DATA);return std::nullopt;
    }
    DWORD ekuBytes=0;
    if(!CertGetEnhancedKeyUsage(cert,CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG,nullptr,&ekuBytes) || ekuBytes>kMaxPairingBytes) {
        Fail(error,"Missing identity TLS purposes");return std::nullopt;
    }
    std::vector<std::uint8_t> ekuStorage(ekuBytes);
    auto* eku=reinterpret_cast<CERT_ENHKEY_USAGE*>(ekuStorage.data());
    if(!CertGetEnhancedKeyUsage(cert,CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG,eku,&ekuBytes)) {
        Fail(error,"Invalid identity TLS purposes");return std::nullopt;
    }
    bool client=false,server=false;
    for(DWORD i=0;i<eku->cUsageIdentifier;++i){
        client|=std::strcmp(eku->rgpszUsageIdentifier[i],szOID_PKIX_KP_CLIENT_AUTH)==0;
        server|=std::strcmp(eku->rgpszUsageIdentifier[i],szOID_PKIX_KP_SERVER_AUTH)==0;
    }
    BYTE usage=0;
    if(!client || !server || !CertGetIntendedKeyUsage(X509_ASN_ENCODING,cert->pCertInfo,&usage,1) ||
        !(usage&CERT_DIGITAL_SIGNATURE_KEY_USAGE)) {
        Fail(error,"Identity must support both TLS roles and signing",ERROR_INVALID_DATA);return std::nullopt;
    }
    PublicPeerIdentity result;
    result.certificate_der.assign(cert->pbCertEncoded,cert->pbCertEncoded+cert->cbCertEncoded);
    for(wchar_t c:wideName)result.stable_name.push_back(static_cast<char>(c));result.fingerprint=Sha256(result.certificate_der);
    if(result.fingerprint.size()!=64){Fail(error,"Identity hash failed");return std::nullopt;}
    return result;
}
bool Generate(std::vector<std::uint8_t>& pfx,PublicPeerIdentity& publicIdentity,std::string& error) {
    std::array<std::uint8_t,16> random{};
    if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
        return Fail(error,"Identity randomness failed");
    static constexpr char hex[]="0123456789abcdef";std::string name="fps-";
    for(auto b:random){name+=hex[b>>4];name+=hex[b&15];}name+=".invalid";
    std::wstring wide(name.begin(),name.end());const auto subject=L"CN="+wide;
    DWORD subjectBytes=0;
    if(!CertStrToNameW(X509_ASN_ENCODING,subject.c_str(),CERT_X500_NAME_STR,nullptr,nullptr,&subjectBytes,nullptr))
        return Fail(error,"Identity subject creation failed");
    std::vector<std::uint8_t> subjectData(subjectBytes);
    if(!CertStrToNameW(X509_ASN_ENCODING,subject.c_str(),CERT_X500_NAME_STR,nullptr,subjectData.data(),&subjectBytes,nullptr))
        return Fail(error,"Identity subject encoding failed");
    CngHandle provider,key;
    SECURITY_STATUS status=NCryptOpenStorageProvider(&provider.h,MS_KEY_STORAGE_PROVIDER,0);
    if(status!=ERROR_SUCCESS)return Fail(error,"Identity provider creation failed",static_cast<DWORD>(status));
    status=NCryptCreatePersistedKey(provider.h,&key.h,NCRYPT_RSA_ALGORITHM,nullptr,0,0);
    if(status!=ERROR_SUCCESS)return Fail(error,"Ephemeral identity key creation failed",static_cast<DWORD>(status));
    DWORD bits=2048,exportPolicy=NCRYPT_ALLOW_EXPORT_FLAG|NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
    if(NCryptSetProperty(key.h,NCRYPT_LENGTH_PROPERTY,reinterpret_cast<PBYTE>(&bits),sizeof(bits),0)!=ERROR_SUCCESS ||
        NCryptSetProperty(key.h,NCRYPT_EXPORT_POLICY_PROPERTY,reinterpret_cast<PBYTE>(&exportPolicy),sizeof(exportPolicy),0)!=ERROR_SUCCESS ||
        NCryptFinalizeKey(key.h,0)!=ERROR_SUCCESS)return Fail(error,"Ephemeral identity key finalization failed");
    CERT_ALT_NAME_ENTRY alt{};alt.dwAltNameChoice=CERT_ALT_NAME_DNS_NAME;alt.pwszDNSName=wide.data();
    CERT_ALT_NAME_INFO names{1,&alt};
    LPSTR purposes[]={const_cast<LPSTR>(szOID_PKIX_KP_CLIENT_AUTH),const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH)};
    CERT_ENHKEY_USAGE eku{2,purposes};BYTE signing=CERT_DIGITAL_SIGNATURE_KEY_USAGE;
    CRYPT_BIT_BLOB usage{1,&signing,7};CERT_BASIC_CONSTRAINTS2_INFO basic{};
    std::array<std::vector<std::uint8_t>,4> data;
    if(!EncodeObject(X509_ALTERNATE_NAME,&names,data[0]) ||
        !EncodeObject(X509_ENHANCED_KEY_USAGE,&eku,data[1]) ||
        !EncodeObject(X509_KEY_USAGE,&usage,data[2]) ||
        !EncodeObject(X509_BASIC_CONSTRAINTS2,&basic,data[3]))return Fail(error,"Identity extension encoding failed");
    std::array<CERT_EXTENSION,4> extensions{};
    LPCSTR oids[]={szOID_SUBJECT_ALT_NAME2,szOID_ENHANCED_KEY_USAGE,szOID_KEY_USAGE,szOID_BASIC_CONSTRAINTS2};
    for(std::size_t i=0;i<extensions.size();++i){
        extensions[i].pszObjId=const_cast<LPSTR>(oids[i]);extensions[i].fCritical=(i>=2);
        extensions[i].Value={static_cast<DWORD>(data[i].size()),data[i].data()};
    }
    CERT_EXTENSIONS all{static_cast<DWORD>(extensions.size()),extensions.data()};
    CERT_NAME_BLOB subjectBlob{subjectBytes,subjectData.data()};
    CRYPT_ALGORITHM_IDENTIFIER signature{const_cast<LPSTR>(szOID_RSA_SHA256RSA),{}};
    FILETIME now{};GetSystemTimeAsFileTime(&now);ULARGE_INTEGER clock{};clock.LowPart=now.dwLowDateTime;clock.HighPart=now.dwHighDateTime;
    auto begin=clock;begin.QuadPart-=300ULL*10'000'000ULL;FILETIME beginFile{begin.LowPart,begin.HighPart};
    auto end=clock;end.QuadPart+=365ULL*24ULL*3600ULL*10'000'000ULL;FILETIME endFile{end.LowPart,end.HighPart};
    SYSTEMTIME start{},finish{};FileTimeToSystemTime(&beginFile,&start);FileTimeToSystemTime(&endFile,&finish);
    Certificate cert{CertCreateSelfSignCertificate(key.h,&subjectBlob,CERT_CREATE_SELFSIGN_NO_KEY_INFO,nullptr,
        &signature,&start,&finish,&all)};
    if(!cert)return Fail(error,"Self-signed identity certificate creation failed");
    CERT_KEY_CONTEXT keyContext{};keyContext.cbSize=sizeof(keyContext);keyContext.hNCryptKey=key.h;keyContext.dwKeySpec=CERT_NCRYPT_KEY_SPEC;
    if(!CertSetCertificateContextProperty(cert.get(),CERT_KEY_CONTEXT_PROP_ID,
        CERT_STORE_NO_CRYPT_RELEASE_FLAG|CERT_SET_PROPERTY_INHIBIT_PERSIST_FLAG,&keyContext))
        return Fail(error,"Identity key association failed");
    Store store{CertOpenStore(CERT_STORE_PROV_MEMORY,0,0,0,nullptr)};
    if(!store.h || !CertAddCertificateLinkToStore(store.h,cert.get(),CERT_STORE_ADD_ALWAYS,nullptr))
        return Fail(error,"In-memory identity store failed");
    CRYPT_DATA_BLOB blob{};constexpr DWORD flags=EXPORT_PRIVATE_KEYS|REPORT_NO_PRIVATE_KEY|REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY;
    if(!PFXExportCertStoreEx(store.h,&blob,L"",nullptr,flags) || blob.cbData>kMaxPairingBytes)
        return Fail(error,"In-memory identity export failed");
    pfx.resize(blob.cbData);blob.pbData=pfx.data();
    if(!PFXExportCertStoreEx(store.h,&blob,L"",nullptr,flags))return Fail(error,"Identity export failed");
    pfx.resize(blob.cbData);auto parsed=PublicFromCertificate(cert.get(),error);
    if(!parsed)return false;publicIdentity=std::move(*parsed);return true;
}
bool PublicFromPfx(std::span<const std::uint8_t> pfx,PublicPeerIdentity& result,std::string& error) {
    CRYPT_DATA_BLOB blob{static_cast<DWORD>(pfx.size()),const_cast<BYTE*>(pfx.data())};
    Store store{PFXImportCertStore(&blob,L"",PKCS12_NO_PERSIST_KEY|PKCS12_ALWAYS_CNG_KSP)};
    if(!store.h)return Fail(error,"Persisted identity import failed");
    Certificate cert{CertEnumCertificatesInStore(store.h,nullptr)};
    if(!cert)return Fail(error,"Persisted identity has no certificate");
    // NO_PERSIST_KEY stores an ephemeral key context, not provider/container metadata.
    CERT_KEY_CONTEXT keyContext{};DWORD contextSize=sizeof(keyContext);
    if(!CertGetCertificateContextProperty(cert.get(),CERT_KEY_CONTEXT_PROP_ID,&keyContext,&contextSize) ||
        keyContext.dwKeySpec!=CERT_NCRYPT_KEY_SPEC || !keyContext.hNCryptKey)
    {
        std::ostringstream diagnostic;diagnostic<<"Persisted identity key property unavailable; property IDs:";
        DWORD id=0;unsigned count=0;while((id=CertEnumCertificateContextProperties(cert.get(),id))!=0 && count++<64)diagnostic<<' '<<id;
        error=diagnostic.str();return false;
    }
    DWORD privatePublicSize=0;
    if(NCryptExportKey(keyContext.hNCryptKey,0,BCRYPT_RSAPUBLIC_BLOB,nullptr,nullptr,0,&privatePublicSize,0)!=ERROR_SUCCESS ||
        privatePublicSize>16384)return Fail(error,"Persisted identity public-key check failed");
    std::vector<std::uint8_t> privatePublic(privatePublicSize);
    if(NCryptExportKey(keyContext.hNCryptKey,0,BCRYPT_RSAPUBLIC_BLOB,nullptr,privatePublic.data(),privatePublicSize,&privatePublicSize,0)!=ERROR_SUCCESS)
        return Fail(error,"Persisted identity public-key export failed");
    BCRYPT_KEY_HANDLE certificateKey=nullptr;
    if(!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING,&cert->pCertInfo->SubjectPublicKeyInfo,0,nullptr,&certificateKey))
        return Fail(error,"Persisted certificate public-key import failed");
    ULONG certificatePublicSize=0;
    auto status=BCryptExportKey(certificateKey,nullptr,BCRYPT_RSAPUBLIC_BLOB,nullptr,0,&certificatePublicSize,0);
    std::vector<std::uint8_t> certificatePublic;
    if(status==0 && certificatePublicSize<=16384) {
        certificatePublic.resize(certificatePublicSize);
        status=BCryptExportKey(certificateKey,nullptr,BCRYPT_RSAPUBLIC_BLOB,certificatePublic.data(),certificatePublicSize,&certificatePublicSize,0);
    } else status=static_cast<NTSTATUS>(-1);
    BCryptDestroyKey(certificateKey);
    if(status!=0 || certificatePublic!=privatePublic)return Fail(error,"Persisted identity key does not match certificate",ERROR_INVALID_DATA);
    auto parsed=PublicFromCertificate(cert.get(),error);if(!parsed)return false;result=std::move(*parsed);return true;
}
void U32(std::vector<std::uint8_t>& out,std::uint32_t v){for(int shift=24;shift>=0;shift-=8)out.push_back(static_cast<std::uint8_t>(v>>shift));}
std::uint32_t ReadU32(std::span<const std::uint8_t> data,std::size_t offset){
    return (static_cast<std::uint32_t>(data[offset])<<24)|(static_cast<std::uint32_t>(data[offset+1])<<16)|
        (static_cast<std::uint32_t>(data[offset+2])<<8)|data[offset+3];
}
}
std::string Sha256(std::span<const std::uint8_t> bytes) {
    if(bytes.size()>std::numeric_limits<ULONG>::max())return {};
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)!=0)return {};
    std::array<std::uint8_t,32> digest{};
    const auto status=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),
        digest.data(),static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);if(status!=0)return {};
    constexpr char hex[]="0123456789ABCDEF";std::string value;value.reserve(64);
    for(auto b:digest){value+=hex[b>>4];value+=hex[b&15];}return value;
}
std::vector<std::uint8_t> EncodePeerBundle(const PublicPeerIdentity& peer) {
    if(peer.certificate_der.empty() || peer.stable_name.size()>256 || peer.certificate_der.size()+peer.stable_name.size()+12>kMaxPairingBytes)return {};
    std::vector<std::uint8_t> out{'F','P','P',1};U32(out,static_cast<std::uint32_t>(peer.stable_name.size()));U32(out,static_cast<std::uint32_t>(peer.certificate_der.size()));
    out.insert(out.end(),peer.stable_name.begin(),peer.stable_name.end());out.insert(out.end(),peer.certificate_der.begin(),peer.certificate_der.end());return out;
}
std::optional<PublicPeerIdentity> ParsePeerBundle(std::span<const std::uint8_t> bytes,std::string& error) {
    if(bytes.size()<12 || bytes.size()>kMaxPairingBytes || bytes[0]!='F'||bytes[1]!='P'||bytes[2]!='P'||bytes[3]!=1){
        Fail(error,"Invalid public pairing format",ERROR_INVALID_DATA);return std::nullopt;
    }
    const auto nameSize=ReadU32(bytes,4),certSize=ReadU32(bytes,8);
    if(nameSize==0 || nameSize>256 || certSize==0 || static_cast<std::uint64_t>(nameSize)+certSize+12!=bytes.size()){
        Fail(error,"Invalid public pairing length",ERROR_INVALID_DATA);return std::nullopt;
    }
    Certificate cert{CertCreateCertificateContext(X509_ASN_ENCODING,bytes.data()+12+nameSize,certSize)};
    if(!cert || cert->cbCertEncoded!=certSize){Fail(error,"Invalid public pairing certificate");return std::nullopt;}
    auto result=PublicFromCertificate(cert.get(),error);
    if(!result)return std::nullopt;
    const std::string name(reinterpret_cast<const char*>(bytes.data()+12),nameSize);
    if(name!=result->stable_name){Fail(error,"Pairing name does not match certificate SAN",ERROR_INVALID_DATA);return std::nullopt;}
    return result;
}
bool ValidateTrustedPeer(const PublicPeerIdentity& own,const PublicPeerIdentity& peer,std::string& error) {
    auto parsed=ParsePeerBundle(EncodePeerBundle(peer),error);
    if(!parsed || parsed->fingerprint!=peer.fingerprint)return Fail(error,"Unusable peer identity",ERROR_INVALID_DATA);
    if(own.fingerprint==peer.fingerprint)return Fail(error,"Cannot pair with the same identity; use separate data folders",ERROR_INVALID_DATA);
    return true;
}
bool WritePublicBundle(const std::filesystem::path& path,const PublicPeerIdentity& peer,std::string& error) {
    auto bytes=EncodePeerBundle(peer);return WriteAtomic(path,bytes,true,error);
}
std::optional<PublicPeerIdentity> ReadPublicBundle(const std::filesystem::path& path,std::string& error) {
    std::vector<std::uint8_t> bytes;if(!ReadBytes(path,bytes,error))return std::nullopt;return ParsePeerBundle(bytes,error);
}
bool SaveTrustedPeer(const std::filesystem::path& path,const PublicPeerIdentity& peer,std::string& error) {
    return WritePublicBundle(path,peer,error);
}
std::unique_ptr<PeerIdentity> PeerIdentity::LoadOrCreate(const std::filesystem::path& directory,std::string& error) {
    auto result=std::unique_ptr<PeerIdentity>{new PeerIdentity};
    const auto path=directory/"identity.dpapi";std::error_code ec;
    const bool exists=std::filesystem::exists(path,ec);
    if(ec){Fail(error,"Identity file access failed",static_cast<DWORD>(ec.value()));return nullptr;}
    if(exists) {
        std::vector<std::uint8_t> protectedBytes;if(!ReadBytes(path,protectedBytes,error))return nullptr;
        DATA_BLOB input{static_cast<DWORD>(protectedBytes.size()),protectedBytes.data()};ProtectedBlob plain;
        if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&plain.blob)){
            Fail(error,"Identity cannot be decrypted for this Windows user");return nullptr;
        }
        if(plain.blob.cbData==0 || plain.blob.cbData>kMaxPairingBytes){Fail(error,"Invalid protected identity length",ERROR_INVALID_DATA);return nullptr;}
        result->pfx_.assign(plain.blob.pbData,plain.blob.pbData+plain.blob.cbData);
        if(!PublicFromPfx(result->pfx_,result->public_,error))return nullptr;
    } else {
        if(!Generate(result->pfx_,result->public_,error))return nullptr;
        DATA_BLOB input{static_cast<DWORD>(result->pfx_.size()),result->pfx_.data()};ProtectedBlob protectedData;
        if(!CryptProtectData(&input,L"FriendPushScreen identity",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&protectedData.blob)){
            Fail(error,"Identity encryption failed");return nullptr;
        }
        if(!WriteAtomic(path,{protectedData.blob.pbData,protectedData.blob.cbData},false,error))return nullptr;
    }
    return result;
}
PeerIdentity::~PeerIdentity(){if(!pfx_.empty())SecureZeroMemory(pfx_.data(),pfx_.size());}
}
