#ifndef YA_UTILS_H
#define YA_UTILS_H

#include <chrono>
#include <map>
#include <string>

namespace ya {

class YaUtils {
 public:
  class Exe {
   public:
    Exe() = default;
    ~Exe() = default;
    static std::string GetExeDir();
    static std::string GetExeName();
    static std::string GetExePath();
    static void ProcessArgs(int argc, char* argv[]);

   private:
    static std::string m_exe_name;
  };

  class Timer {
   public:
    Timer() = default;
    ~Timer() = default;
    static void StartTimer(const std::string& timer_id = "0");
    static double GetElapsedTime_ms(const std::string& timer_id = "0");
    static double GetElapsedTime_s(const std::string& timer_id = "0");
    static double GetElapsedTime_μs(const std::string& timer_id = "0");
    static void Sleep(unsigned int ms);

   private:
    static std::map<std::string, std::chrono::steady_clock::time_point>
        m_timers;
  };

  class Platform {
   public:
    enum class PLATFORM { WINDOWS, LINUX, MACOS };
    Platform() = default;
    ~Platform() = default;

   public:
    static PLATFORM GetPlatform();
  };

  using CryptoBuffer = std::vector<unsigned char>;

  class Crypto {
   public:
    Crypto();
    ~Crypto();

    // AES
    bool AES_Encrypt(std::string_view plaintext, std::string_view key,
                     std::string_view iv, CryptoBuffer& ciphertext);
    bool AES_Decrypt(const CryptoBuffer& ciphertext, std::string_view key,
                     std::string_view iv, std::string& plaintext);

    // DES
    bool DES_Encrypt(std::string_view plaintext, std::string_view key,
                     std::string_view iv, CryptoBuffer& ciphertext);
    bool DES_Decrypt(const CryptoBuffer& ciphertext, std::string_view key,
                     std::string_view iv, std::string& plaintext);

    // Hash
    bool MD5_Hash(std::string_view data, CryptoBuffer& digest);
    bool SHA256_Hash(std::string_view data, CryptoBuffer& digest);

    // RSA
    bool Generate_RSA_Key(CryptoBuffer& public_key, CryptoBuffer& private_key);
    std::optional<CryptoBuffer> RSA_Encrypt(const CryptoBuffer& public_key,
                                            std::string_view plaintext);
    std::optional<std::string> RSA_Decrypt(const CryptoBuffer& private_key,
                                           const CryptoBuffer& ciphertext);

    // DSA
    bool Generate_DSA_Key(CryptoBuffer& public_key, CryptoBuffer& private_key);
    std::optional<CryptoBuffer> DSA_Sign(const CryptoBuffer& private_key,
                                         std::string_view data);
    bool DSA_Verify(const CryptoBuffer& public_key, std::string_view data,
                    const CryptoBuffer& signature);

   private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
  };
};

}  // namespace ya

#endif  // !YA_UTILS_H
