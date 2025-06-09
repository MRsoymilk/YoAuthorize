#include "yautils.h"

#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <thread>
#include <vector>

#include "platform_def.h"

namespace ya {

std::string YaUtils::Exe::m_exe_name = "";
std::map<std::string, std::chrono::steady_clock::time_point>
    YaUtils::Timer::m_timers = {};

std::string YaUtils::Exe::GetExeDir() {
  std::string path = std::filesystem::current_path().string();
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

std::string YaUtils::Exe::GetExeName() { return m_exe_name; }

std::string YaUtils::Exe::GetExePath() {
  return GetExeDir() + "/" + GetExeName();
}

void YaUtils::Exe::ProcessArgs(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  std::filesystem::path exe_name = argv[0];
  m_exe_name = exe_name.filename().string();
}

void YaUtils::Timer::Sleep(unsigned int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void YaUtils::Timer::StartTimer(const std::string& timer_id) {
  m_timers[timer_id] = std::chrono::steady_clock::now();
}

double YaUtils::Timer::GetElapsedTime_ms(const std::string& timer_id) {
  return GetElapsedTime_μs(timer_id) / 1'000.0;
}

double YaUtils::Timer::GetElapsedTime_s(const std::string& timer_id) {
  return GetElapsedTime_μs(timer_id) / 1'000'000.0;
}

double YaUtils::Timer::GetElapsedTime_μs(const std::string& timer_id) {
  auto now = std::chrono::steady_clock::now();
  auto it = m_timers.find(timer_id);
  if (it == m_timers.end()) {
    return 0.0;
  }
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(now - it->second);
  return static_cast<double>(duration.count());
}

YaUtils::Platform::PLATFORM YaUtils::Platform::GetPlatform() {
#if defined(YA_WINDOWS)
  return PLATFORM::WINDOWS;
#elif defined(YA_LINUX)
  return PLATFORM::LINUX;
#elif defined(YA_MACOS)
  return PLATFORM::MACOS;
#else
#error "Unsupported platform"
#endif
}

class YaUtils::Crypto::Impl {
 public:
  Impl() {
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
  }

  ~Impl() {
    EVP_cleanup();
    ERR_free_strings();
  }

  bool Cipher_Encrypt(std::string_view plaintext, CryptoBuffer& ciphertext,
                      std::string_view key, std::string_view iv,
                      const EVP_CIPHER* cipher) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (!EVP_EncryptInit_ex(
            ctx, cipher, nullptr,
            reinterpret_cast<const unsigned char*>(key.data()),
            reinterpret_cast<const unsigned char*>(iv.data()))) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }

    std::vector<unsigned char> buffer(plaintext.size() +
                                      EVP_CIPHER_block_size(cipher));
    int len = 0, ciphertext_len = 0;
    if (!EVP_EncryptUpdate(
            ctx, buffer.data(), &len,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            plaintext.size())) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }
    ciphertext_len = len;

    if (!EVP_EncryptFinal_ex(ctx, buffer.data() + len, &len)) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }
    ciphertext_len += len;
    buffer.resize(ciphertext_len);
    ciphertext = std::move(buffer);

    EVP_CIPHER_CTX_free(ctx);
    return true;
  }

  bool Cipher_Decrypt(const CryptoBuffer& ciphertext, std::string& plaintext,
                      std::string_view key, std::string_view iv,
                      const EVP_CIPHER* cipher) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (!EVP_DecryptInit_ex(
            ctx, cipher, nullptr,
            reinterpret_cast<const unsigned char*>(key.data()),
            reinterpret_cast<const unsigned char*>(iv.data()))) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }

    std::vector<unsigned char> buffer(ciphertext.size());
    int len = 0, plaintext_len = 0;
    if (!EVP_DecryptUpdate(ctx, buffer.data(), &len, ciphertext.data(),
                           ciphertext.size())) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }
    plaintext_len = len;

    if (!EVP_DecryptFinal_ex(ctx, buffer.data() + len, &len)) {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }
    plaintext_len += len;
    buffer.resize(plaintext_len);
    plaintext.assign(buffer.begin(), buffer.end());

    EVP_CIPHER_CTX_free(ctx);
    return true;
  }

  bool Digest_Hash(std::string_view data, CryptoBuffer& digest,
                   const EVP_MD* md) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (!EVP_DigestInit_ex(ctx, md, nullptr)) {
      EVP_MD_CTX_free(ctx);
      return false;
    }

    if (!EVP_DigestUpdate(ctx, data.data(), data.size())) {
      EVP_MD_CTX_free(ctx);
      return false;
    }

    digest.resize(EVP_MD_size(md));
    unsigned int len = 0;
    if (!EVP_DigestFinal_ex(ctx, digest.data(), &len)) {
      EVP_MD_CTX_free(ctx);
      return false;
    }
    digest.resize(len);

    EVP_MD_CTX_free(ctx);
    return true;
  }

  bool Generate_KeyPair_RSA(CryptoBuffer& pub, CryptoBuffer& pri) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) return false;
    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      return false;
    }

    BIO* pub_bio = BIO_new(BIO_s_mem());
    BIO* pri_bio = BIO_new(BIO_s_mem());
    if (!pub_bio || !pri_bio) {
      EVP_PKEY_free(pkey);
      EVP_PKEY_CTX_free(ctx);
      BIO_free(pub_bio);
      BIO_free(pri_bio);
      return false;
    }

    if (!PEM_write_bio_PUBKEY(pub_bio, pkey) ||
        !PEM_write_bio_PrivateKey(pri_bio, pkey, nullptr, nullptr, 0, nullptr,
                                  nullptr)) {
      EVP_PKEY_free(pkey);
      EVP_PKEY_CTX_free(ctx);
      BIO_free(pub_bio);
      BIO_free(pri_bio);
      return false;
    }

    pub = Read_BIO(pub_bio);
    pri = Read_BIO(pri_bio);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    BIO_free(pub_bio);
    BIO_free(pri_bio);
    return true;
  }

  std::optional<CryptoBuffer> RSA_Encrypt(const CryptoBuffer& pub,
                                          std::string_view plaintext) {
    BIO* bio = BIO_new_mem_buf(pub.data(), pub.size());
    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if (!key) {
      BIO_free(bio);
      return std::nullopt;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
    if (!ctx || EVP_PKEY_encrypt_init(ctx) <= 0) {
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    size_t outlen = 0;
    if (EVP_PKEY_encrypt(
            ctx, nullptr, &outlen,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            plaintext.size()) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    CryptoBuffer ciphertext(outlen);
    if (EVP_PKEY_encrypt(
            ctx, ciphertext.data(), &outlen,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            plaintext.size()) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }
    ciphertext.resize(outlen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(key);
    BIO_free(bio);
    return ciphertext;
  }

  std::optional<std::string> RSA_Decrypt(const CryptoBuffer& pri,
                                         const CryptoBuffer& ciphertext) {
    BIO* bio = BIO_new_mem_buf(pri.data(), pri.size());
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    if (!key) {
      BIO_free(bio);
      return std::nullopt;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
    if (!ctx || EVP_PKEY_decrypt_init(ctx) <= 0) {
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, ciphertext.data(),
                         ciphertext.size()) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    CryptoBuffer buffer(outlen);
    if (EVP_PKEY_decrypt(ctx, buffer.data(), &outlen, ciphertext.data(),
                         ciphertext.size()) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }
    buffer.resize(outlen);
    std::string plaintext(buffer.begin(), buffer.end());

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(key);
    BIO_free(bio);
    return plaintext;
  }

  bool Generate_KeyPair_DSA(CryptoBuffer& pub, CryptoBuffer& pri) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, nullptr);
    if (!ctx) return false;
    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_dsa_paramgen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
      EVP_PKEY_CTX_free(ctx);
      return false;
    }

    BIO* pub_bio = BIO_new(BIO_s_mem());
    BIO* pri_bio = BIO_new(BIO_s_mem());
    if (!pub_bio || !pri_bio) {
      EVP_PKEY_free(pkey);
      EVP_PKEY_CTX_free(ctx);
      BIO_free(pub_bio);
      BIO_free(pri_bio);
      return false;
    }

    if (!PEM_write_bio_PUBKEY(pub_bio, pkey) ||
        !PEM_write_bio_PrivateKey(pri_bio, pkey, nullptr, nullptr, 0, nullptr,
                                  nullptr)) {
      EVP_PKEY_free(pkey);
      EVP_PKEY_CTX_free(ctx);
      BIO_free(pub_bio);
      BIO_free(pri_bio);
      return false;
    }

    pub = Read_BIO(pub_bio);
    pri = Read_BIO(pri_bio);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    BIO_free(pub_bio);
    BIO_free(pri_bio);
    return true;
  }

  std::optional<CryptoBuffer> DSA_Sign(const CryptoBuffer& pri,
                                       std::string_view data) {
    BIO* bio = BIO_new_mem_buf(pri.data(), pri.size());
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    if (!key) {
      BIO_free(bio);
      return std::nullopt;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx || !EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, key)) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    if (!EVP_DigestSignUpdate(ctx, data.data(), data.size())) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    size_t sig_len = 0;
    if (!EVP_DigestSignFinal(ctx, nullptr, &sig_len)) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }

    CryptoBuffer signature(sig_len);
    if (!EVP_DigestSignFinal(ctx, signature.data(), &sig_len)) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return std::nullopt;
    }
    signature.resize(sig_len);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    BIO_free(bio);
    return signature;
  }

  bool DSA_Verify(const CryptoBuffer& pub, std::string_view data,
                  const CryptoBuffer& signature) {
    BIO* bio = BIO_new_mem_buf(pub.data(), pub.size());
    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if (!key) {
      BIO_free(bio);
      return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx ||
        !EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key)) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return false;
    }

    if (!EVP_DigestVerifyUpdate(ctx, data.data(), data.size())) {
      EVP_MD_CTX_free(ctx);
      EVP_PKEY_free(key);
      BIO_free(bio);
      return false;
    }

    bool result =
        EVP_DigestVerifyFinal(ctx, signature.data(), signature.size()) == 1;

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    BIO_free(bio);
    return result;
  }

 private:
  CryptoBuffer Read_BIO(BIO* bio) {
    CryptoBuffer buf;
    char tmp[256];
    int len = 0;
    while ((len = BIO_read(bio, tmp, sizeof(tmp))) > 0) {
      buf.insert(buf.end(), tmp, tmp + len);
    }
    return buf;
  }
};

YaUtils::Crypto::Crypto() : m_impl(std::make_unique<Impl>()) {}

YaUtils::Crypto::~Crypto() = default;

bool YaUtils::Crypto::AES_Encrypt(std::string_view plaintext,
                                  std::string_view key, std::string_view iv,
                                  CryptoBuffer& ciphertext) {
  return m_impl->Cipher_Encrypt(plaintext, ciphertext, key, iv,
                                EVP_aes_256_cbc());
}

bool YaUtils::Crypto::AES_Decrypt(const CryptoBuffer& ciphertext,
                                  std::string_view key, std::string_view iv,
                                  std::string& plaintext) {
  return m_impl->Cipher_Decrypt(ciphertext, plaintext, key, iv,
                                EVP_aes_256_cbc());
}

bool YaUtils::Crypto::DES_Encrypt(std::string_view plaintext,
                                  std::string_view key, std::string_view iv,
                                  CryptoBuffer& ciphertext) {
  return m_impl->Cipher_Encrypt(plaintext, ciphertext, key, iv, EVP_des_cbc());
}

bool YaUtils::Crypto::DES_Decrypt(const CryptoBuffer& ciphertext,
                                  std::string_view key, std::string_view iv,
                                  std::string& plaintext) {
  return m_impl->Cipher_Decrypt(ciphertext, plaintext, key, iv, EVP_des_cbc());
}

bool YaUtils::Crypto::MD5_Hash(std::string_view data, CryptoBuffer& digest) {
  return m_impl->Digest_Hash(data, digest, EVP_md5());
}

bool YaUtils::Crypto::SHA256_Hash(std::string_view data, CryptoBuffer& digest) {
  return m_impl->Digest_Hash(data, digest, EVP_sha256());
}

bool YaUtils::Crypto::Generate_RSA_Key(CryptoBuffer& public_key,
                                       CryptoBuffer& private_key) {
  return m_impl->Generate_KeyPair_RSA(public_key, private_key);
}

std::optional<YaUtils::CryptoBuffer> YaUtils::Crypto::RSA_Encrypt(
    const CryptoBuffer& public_key, std::string_view plaintext) {
  return m_impl->RSA_Encrypt(public_key, plaintext);
}

std::optional<std::string> YaUtils::Crypto::RSA_Decrypt(
    const CryptoBuffer& private_key, const CryptoBuffer& ciphertext) {
  return m_impl->RSA_Decrypt(private_key, ciphertext);
}

bool YaUtils::Crypto::Generate_DSA_Key(CryptoBuffer& public_key,
                                       CryptoBuffer& private_key) {
  return m_impl->Generate_KeyPair_DSA(public_key, private_key);
}

std::optional<YaUtils::CryptoBuffer> YaUtils::Crypto::DSA_Sign(
    const CryptoBuffer& private_key, std::string_view data) {
  return m_impl->DSA_Sign(private_key, data);
}

bool YaUtils::Crypto::DSA_Verify(const CryptoBuffer& public_key,
                                 std::string_view data,
                                 const CryptoBuffer& signature) {
  return m_impl->DSA_Verify(public_key, data, signature);
}
}  // namespace ya
