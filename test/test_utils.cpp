#include <gtest/gtest.h>

#include <format>
#include <iostream>

#include "yautils.h"

class ExeUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // simulate
    const char* argv0 = "./test_app";
    ya::YaUtils::Exe::ProcessArgs(1, const_cast<char**>(&argv0));
  }
};

TEST_F(ExeUtilsTest, ExePathValid) {
  std::string path = ya::YaUtils::Exe::GetExePath();
  EXPECT_FALSE(path.empty());
  std::cout << "Exe Path: " << path << std::endl;
}

TEST_F(ExeUtilsTest, ExeNameValid) {
  std::string name = ya::YaUtils::Exe::GetExeName();
  EXPECT_FALSE(name.empty());
  std::cout << std::format("App Name: {}", name) << std::endl;
}

TEST_F(ExeUtilsTest, ExeDirValid) {
  std::string dir = ya::YaUtils::Exe::GetExeDir();
  EXPECT_FALSE(dir.empty());
  std::cout << std::format("App Dir: {}", dir) << std::endl;
}

TEST_F(ExeUtilsTest, FullPathConsistent) {
  std::string path = ya::YaUtils::Exe::GetExePath();
  std::string dir = ya::YaUtils::Exe::GetExeDir();
  std::string name = ya::YaUtils::Exe::GetExeName();
  EXPECT_TRUE(path.find(dir) != std::string::npos);
  EXPECT_TRUE(path.find(name) != std::string::npos);
  std::cout << std::format("Full Path: {}", path) << std::endl;
}

TEST(TimerTest, SleepAccuracy) {
  ya::YaUtils::Timer::StartTimer();
  ya::YaUtils::Timer::Sleep(1000);  // sleep 1 second

  int64_t elapsed_us = ya::YaUtils::Timer::GetElapsedTime_μs();
  int64_t elapsed_ms = ya::YaUtils::Timer::GetElapsedTime_ms();
  double elapsed_s = ya::YaUtils::Timer::GetElapsedTime_s();

  std::cout << std::format("Elapsed: {} μs", elapsed_us) << std::endl;
  std::cout << std::format("Elapsed: {} ms", elapsed_ms) << std::endl;
  std::cout << std::format("Elapsed: {} s", elapsed_s) << std::endl;

  EXPECT_GE(elapsed_ms, 950);
  EXPECT_LE(elapsed_ms, 1100);
}

TEST(PlatformTest, Macro) {
  auto platform = ya::YaUtils::Platform::GetPlatform();
  switch (platform) {
    case ya::YaUtils::Platform::PLATFORM::WINDOWS:
      std::cout << "Platform: Windows" << std::endl;
      break;
    case ya::YaUtils::Platform::PLATFORM::LINUX:
      std::cout << "Platform: Linux" << std::endl;
      break;
    case ya::YaUtils::Platform::PLATFORM::MACOS:
      std::cout << "Platform: MACOS" << std::endl;
      break;
    default:
      std::cout << "Unkndown" << std::endl;
      break;
  }
}

// Test fixture for Crypto class
class CryptoTest : public ::testing::Test {
 protected:
  ya::YaUtils::Crypto crypto;
  std::string plaintext = "Hello, World!";
  // AES-256: 32-byte key, 16-byte IV
  std::string aes_key = std::string(32, 'A');
  std::string aes_iv = std::string(16, 'B');
  // DES: 8-byte key, 8-byte IV
  std::string des_key = std::string(8, 'C');
  std::string des_iv = std::string(8, 'D');
};

// AES Tests
TEST_F(CryptoTest, AES_EncryptDecrypt_Success) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.AES_Encrypt(plaintext, aes_key, aes_iv, ciphertext));
  EXPECT_FALSE(ciphertext.empty());

  std::string decrypted;
  ASSERT_TRUE(crypto.AES_Decrypt(ciphertext, aes_key, aes_iv, decrypted));
  EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoTest, AES_EncryptDecrypt_WrongKey) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.AES_Encrypt(plaintext, aes_key, aes_iv, ciphertext));

  std::string wrong_key = std::string(32, 'X');
  std::string decrypted;
  ASSERT_FALSE(crypto.AES_Decrypt(ciphertext, wrong_key, aes_iv, decrypted));
}

TEST_F(CryptoTest, AES_EncryptDecrypt_WrongIV) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.AES_Encrypt(plaintext, aes_key, aes_iv, ciphertext));

  std::string wrong_iv = std::string(16, 'Y');
  std::string decrypted;
  ASSERT_FALSE(crypto.AES_Decrypt(ciphertext, aes_key, wrong_iv, decrypted));
}

// DES Tests
TEST_F(CryptoTest, DES_EncryptDecrypt_Success) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.DES_Encrypt(plaintext, des_key, des_iv, ciphertext));
  EXPECT_FALSE(ciphertext.empty());

  std::string decrypted;
  ASSERT_TRUE(crypto.DES_Decrypt(ciphertext, des_key, des_iv, decrypted));
  EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoTest, DES_EncryptDecrypt_WrongKey) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.DES_Encrypt(plaintext, des_key, des_iv, ciphertext));

  std::string wrong_key = std::string(8, 'X');
  std::string decrypted;
  ASSERT_FALSE(crypto.DES_Decrypt(ciphertext, wrong_key, des_iv, decrypted));
}

TEST_F(CryptoTest, DES_EncryptDecrypt_WrongIV) {
  ya::YaUtils::CryptoBuffer ciphertext;
  ASSERT_TRUE(crypto.DES_Encrypt(plaintext, des_key, des_iv, ciphertext));

  std::string wrong_iv = std::string(8, 'Y');
  std::string decrypted;
  ASSERT_FALSE(crypto.DES_Decrypt(ciphertext, des_key, wrong_iv, decrypted));
}

// Hash Tests
TEST_F(CryptoTest, MD5_Hash_Success) {
  ya::YaUtils::CryptoBuffer digest;
  ASSERT_TRUE(crypto.MD5_Hash(plaintext, digest));
  EXPECT_EQ(digest.size(), 16);  // MD5 produces 128-bit (16-byte) hash
}

TEST_F(CryptoTest, SHA256_Hash_Success) {
  ya::YaUtils::CryptoBuffer digest;
  ASSERT_TRUE(crypto.SHA256_Hash(plaintext, digest));
  EXPECT_EQ(digest.size(), 32);  // SHA256 produces 256-bit (32-byte) hash
}

TEST_F(CryptoTest, MD5_Hash_EmptyInput) {
  ya::YaUtils::CryptoBuffer digest;
  ASSERT_TRUE(crypto.MD5_Hash("", digest));
  EXPECT_EQ(digest.size(), 16);
}

TEST_F(CryptoTest, SHA256_Hash_EmptyInput) {
  ya::YaUtils::CryptoBuffer digest;
  ASSERT_TRUE(crypto.SHA256_Hash("", digest));
  EXPECT_EQ(digest.size(), 32);
}

// RSA Tests
TEST_F(CryptoTest, RSA_GenerateKey_Success) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_RSA_Key(public_key, private_key));
  EXPECT_FALSE(public_key.empty());
  EXPECT_FALSE(private_key.empty());
}

TEST_F(CryptoTest, RSA_EncryptDecrypt_Success) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_RSA_Key(public_key, private_key));

  auto ciphertext = crypto.RSA_Encrypt(public_key, plaintext);
  ASSERT_TRUE(ciphertext.has_value());
  EXPECT_FALSE(ciphertext->empty());

  auto decrypted = crypto.RSA_Decrypt(private_key, *ciphertext);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(CryptoTest, RSA_Encrypt_WrongKey) {
  ya::YaUtils::CryptoBuffer invalid_key(100, 'X');  // Invalid key data
  auto ciphertext = crypto.RSA_Encrypt(invalid_key, plaintext);
  EXPECT_FALSE(ciphertext.has_value());
}

TEST_F(CryptoTest, RSA_Decrypt_WrongKey) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_RSA_Key(public_key, private_key));

  auto ciphertext = crypto.RSA_Encrypt(public_key, plaintext);
  ASSERT_TRUE(ciphertext.has_value());

  ya::YaUtils::CryptoBuffer wrong_key(100, 'X');  // Invalid private key
  auto decrypted = crypto.RSA_Decrypt(wrong_key, *ciphertext);
  EXPECT_FALSE(decrypted.has_value());
}

// DSA Tests
TEST_F(CryptoTest, DSA_GenerateKey_Success) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_DSA_Key(public_key, private_key));
  EXPECT_FALSE(public_key.empty());
  EXPECT_FALSE(private_key.empty());
}

TEST_F(CryptoTest, DSA_SignVerify_Success) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_DSA_Key(public_key, private_key));

  auto signature = crypto.DSA_Sign(private_key, plaintext);
  ASSERT_TRUE(signature.has_value());
  EXPECT_FALSE(signature->empty());

  ASSERT_TRUE(crypto.DSA_Verify(public_key, plaintext, *signature));
}

TEST_F(CryptoTest, DSA_Verify_WrongSignature) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_DSA_Key(public_key, private_key));

  auto signature = crypto.DSA_Sign(private_key, plaintext);
  ASSERT_TRUE(signature.has_value());

  // Modify signature to make it invalid
  ya::YaUtils::CryptoBuffer wrong_signature = *signature;
  if (!wrong_signature.empty()) {
    wrong_signature[0] ^= 0xFF;  // Flip first byte
  }
  EXPECT_FALSE(crypto.DSA_Verify(public_key, plaintext, wrong_signature));
}

TEST_F(CryptoTest, DSA_Sign_WrongKey) {
  ya::YaUtils::CryptoBuffer invalid_key(100, 'X');  // Invalid private key
  auto signature = crypto.DSA_Sign(invalid_key, plaintext);
  EXPECT_FALSE(signature.has_value());
}

TEST_F(CryptoTest, DSA_Verify_WrongKey) {
  ya::YaUtils::CryptoBuffer public_key, private_key;
  ASSERT_TRUE(crypto.Generate_DSA_Key(public_key, private_key));

  auto signature = crypto.DSA_Sign(private_key, plaintext);
  ASSERT_TRUE(signature.has_value());

  ya::YaUtils::CryptoBuffer wrong_key(100, 'X');  // Invalid public key
  EXPECT_FALSE(crypto.DSA_Verify(wrong_key, plaintext, *signature));
}
