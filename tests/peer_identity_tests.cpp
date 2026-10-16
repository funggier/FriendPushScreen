#include "test_support.h"
#include "network/peer_identity.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iterator>
#include <wincrypt.h>
#include <ncrypt.h>
#include <array>
#include <cstring>
namespace {
enum class CertificateFault {Expired,WrongSan,WrongPurpose};
// Re-sign the existing public key with its actual ephemeral private key.
// These are valid signatures with deliberately invalid TLS identity attributes.
fps::network::PublicPeerIdentity SignedCertificateFault(const fps::network::PeerIdentity& identity,CertificateFault fault) {
    using namespace fps::network;
    CRYPT_DATA_BLOB blob{static_cast<DWORD>(identity.PfxBytes().size()),const_cast<BYTE*>(identity.PfxBytes().data())};
    HCERTSTORE store=PFXImportCertStore(&blob,L"",PKCS12_NO_PERSIST_KEY|PKCS12_ALWAYS_CNG_KSP);
    FPS_EXPECT_TRUE(store!=nullptr);if(!store)return {};
    PCCERT_CONTEXT cert=CertEnumCertificatesInStore(store,nullptr);
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key{};DWORD key_spec=0;BOOL free_key=FALSE;
    CERT_KEY_CONTEXT key_context{};DWORD context_size=sizeof(key_context);
    const bool acquired=cert&&CertGetCertificateContextProperty(cert,CERT_KEY_CONTEXT_PROP_ID,&key_context,&context_size)&&key_context.dwKeySpec==CERT_NCRYPT_KEY_SPEC;
    key=key_context.hNCryptKey;key_spec=key_context.dwKeySpec;
    FPS_EXPECT_TRUE(acquired);if(!acquired){if(cert)CertFreeCertificateContext(cert);CertCloseStore(store,0);return {};}
    CERT_INFO info=*cert->pCertInfo;
    std::vector<CERT_EXTENSION> extensions(info.rgExtension,info.rgExtension+info.cExtension);
    std::vector<BYTE> changed;std::wstring dns=L"unrelated.invalid";
    CERT_ALT_NAME_ENTRY entry{};entry.dwAltNameChoice=CERT_ALT_NAME_DNS_NAME;entry.pwszDNSName=dns.data();
    CERT_ALT_NAME_INFO names{1,&entry};
    LPSTR purpose=const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);CERT_ENHKEY_USAGE eku{1,&purpose};
    if(fault==CertificateFault::Expired) {
        SYSTEMTIME old{};old.wYear=2001;old.wMonth=1;old.wDay=1;SystemTimeToFileTime(&old,&info.NotBefore);
        old.wYear=2002;SystemTimeToFileTime(&old,&info.NotAfter);
    }else {
        LPCSTR type=fault==CertificateFault::WrongSan?X509_ALTERNATE_NAME:X509_ENHANCED_KEY_USAGE;
        const void* object=fault==CertificateFault::WrongSan?static_cast<const void*>(&names):static_cast<const void*>(&eku);
        DWORD size=0;const bool sized=CryptEncodeObjectEx(X509_ASN_ENCODING,type,object,0,nullptr,nullptr,&size)!=FALSE;
        FPS_EXPECT_TRUE(sized);changed.resize(size);
        const bool encoded=sized&&CryptEncodeObjectEx(X509_ASN_ENCODING,type,object,0,nullptr,changed.data(),&size);
        FPS_EXPECT_TRUE(encoded);
        for(auto& extension:extensions) {
            const char* oid=fault==CertificateFault::WrongSan?szOID_SUBJECT_ALT_NAME2:szOID_ENHANCED_KEY_USAGE;
            if(std::strcmp(extension.pszObjId,oid)==0)extension.Value={size,changed.data()};
        }
        info.rgExtension=extensions.data();
    }
    DWORD size=0;
    const bool sized=CryptSignAndEncodeCertificate(key,key_spec,X509_ASN_ENCODING,X509_CERT_TO_BE_SIGNED,&info,&info.SignatureAlgorithm,nullptr,nullptr,&size)!=FALSE;
    FPS_EXPECT_TRUE(sized);PublicPeerIdentity result;result.certificate_der.resize(size);
    const bool signed_ok=sized&&CryptSignAndEncodeCertificate(key,key_spec,X509_ASN_ENCODING,X509_CERT_TO_BE_SIGNED,&info,&info.SignatureAlgorithm,nullptr,result.certificate_der.data(),&size);
    FPS_EXPECT_TRUE(signed_ok);result.stable_name=identity.Public().stable_name;result.fingerprint=Sha256(result.certificate_der);
    PCCERT_CONTEXT signed_cert=signed_ok?CertCreateCertificateContext(X509_ASN_ENCODING,result.certificate_der.data(),size):nullptr;
    FPS_EXPECT_TRUE(signed_cert&&CryptVerifyCertificateSignatureEx(0,X509_ASN_ENCODING,CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
        const_cast<CERT_CONTEXT*>(signed_cert),CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,const_cast<CERT_CONTEXT*>(signed_cert),0,nullptr));
    if(signed_cert)CertFreeCertificateContext(signed_cert);
    if(free_key)NCryptFreeObject(key);CertFreeCertificateContext(cert);CertCloseStore(store,0);
    return result;
}
}
void RunPeerIdentityTests() {
    using namespace fps::network;
    const auto root=std::filesystem::current_path()/("fps-peer-identity-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    std::filesystem::create_directories(root);
    std::string error;
    auto a=PeerIdentity::LoadOrCreate(root/"a",error);
    FPS_EXPECT_TRUE(a != nullptr);
    if(!a) { std::cerr<<"IDENTITY_CREATE_RED: "<<error<<'\n';std::filesystem::remove_all(root);return; }
    auto b=PeerIdentity::LoadOrCreate(root/"b",error);
    if(!b)std::cerr<<"SECOND_IDENTITY_FAILED: "<<error<<'\n';
    auto again=PeerIdentity::LoadOrCreate(root/"a",error);
    FPS_EXPECT_TRUE(b != nullptr);
    FPS_EXPECT_TRUE(again != nullptr);
    if(!again)std::cerr<<"RELOAD_IDENTITY_FAILED: "<<error<<'\n';
    if(!b || !again) {std::filesystem::remove_all(root);return;}
    FPS_EXPECT_EQ(a->Public().fingerprint,again->Public().fingerprint);
    FPS_EXPECT_TRUE(a->Public().fingerprint != b->Public().fingerprint);
    FPS_EXPECT_EQ(a->Public().fingerprint.size(),std::size_t{64});
    FPS_EXPECT_TRUE(!a->PfxBytes().empty());
    auto bytes=EncodePeerBundle(a->Public());
    FPS_EXPECT_TRUE(bytes.size() <= kMaxPairingBytes);
    auto parsed=ParsePeerBundle(bytes,error);
    FPS_EXPECT_TRUE(parsed.has_value());
    if(parsed) {
        FPS_EXPECT_EQ(parsed->certificate_der,a->Public().certificate_der);
        FPS_EXPECT_EQ(parsed->stable_name,a->Public().stable_name);
        FPS_EXPECT_EQ(parsed->fingerprint,a->Public().fingerprint);
    }
    FPS_EXPECT_TRUE(!ValidateTrustedPeer(a->Public(),a->Public(),error));
    FPS_EXPECT_TRUE(ValidateTrustedPeer(a->Public(),b->Public(),error));
    FPS_EXPECT_TRUE(WritePublicBundle(root/"a.friendpair",a->Public(),error));
    auto fromFile=ReadPublicBundle(root/"a.friendpair",error);
    FPS_EXPECT_TRUE(fromFile && fromFile->fingerprint == a->Public().fingerprint);
    for(auto fault:{CertificateFault::Expired,CertificateFault::WrongSan,CertificateFault::WrongPurpose}) {
        const auto invalid=SignedCertificateFault(*a,fault);
        FPS_EXPECT_TRUE(!ParsePeerBundle(EncodePeerBundle(invalid),error));
        const char* expected=fault==CertificateFault::Expired?"expired":fault==CertificateFault::WrongSan?"stable peer name":"both TLS roles";
        FPS_EXPECT_TRUE(error.find(expected)!=std::string::npos);
    }
    auto wrong_name=a->Public();wrong_name.stable_name[4]=wrong_name.stable_name[4]=='a'?'b':'a';
    FPS_EXPECT_TRUE(!ParsePeerBundle(EncodePeerBundle(wrong_name),error));
    FPS_EXPECT_TRUE(error.find("SAN")!=std::string::npos);
    auto damaged=bytes;damaged[0]^=0xFF;
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged=bytes;damaged.push_back(0);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged=bytes;damaged.resize(10);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged.assign(kMaxPairingBytes+1,0);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    const auto stored=root/"a"/"identity.dpapi";
    std::ifstream input(stored,std::ios::binary);
    std::vector<std::uint8_t> protectedBytes((std::istreambuf_iterator<char>(input)),{});
    input.close();
    FPS_EXPECT_TRUE(!protectedBytes.empty());
    FPS_EXPECT_TRUE(protectedBytes != std::vector<std::uint8_t>(a->PfxBytes().begin(),a->PfxBytes().end()));
    std::ofstream(stored,std::ios::binary|std::ios::trunc)<<"damaged persisted identity";
    FPS_EXPECT_TRUE(!PeerIdentity::LoadOrCreate(root/"a",error));
    std::ifstream after(stored,std::ios::binary);std::string unchanged((std::istreambuf_iterator<char>(after)),{});after.close();
    FPS_EXPECT_EQ(unchanged,std::string{"damaged persisted identity"});
    std::filesystem::remove_all(root);
}
