// SPDX-License-Identifier: GPL-3.0-or-later
#include <crypto/crypto.hpp>
#include <cstdio>
#include <stdexcept>

int main() {
    const std::string key(16,'k'),iv(12,'i'),plain("authenticated control frame");
    const auto encrypted=crypto::aes_encrypt_gcm(plain,key,iv,12);
    unsigned checks=0,failed=0;
    auto check=[&](std::string ciphertext,std::string candidateKey,std::string tag,std::string nonce,bool expected) {
        bool accepted=false;
        try {accepted=crypto::aes_decrypt_gcm(ciphertext,candidateKey,tag,nonce,12)==plain;}
        catch(const std::exception&){}
        ++checks;if(accepted!=expected)++failed;
    };
    check(encrypted.first,key,encrypted.second,iv,true);
    // Reject modified material even if decryption returns bytes. Test rejection,
    // not equality with plaintext: unauthenticated random bytes are also unsafe.
    auto reject=[&](std::string ciphertext,std::string candidateKey,std::string tag,std::string nonce) {
        bool threw=false;try {crypto::aes_decrypt_gcm(ciphertext,candidateKey,tag,nonce,12);}
        catch(const std::exception&){threw=true;}
        ++checks;if(!threw)++failed;
    };
    auto wrong=key;wrong[0]^=1;reject(encrypted.first,wrong,encrypted.second,iv);
    wrong=iv;wrong[0]^=1;reject(encrypted.first,key,encrypted.second,wrong);
    wrong=encrypted.first;wrong[0]^=1;reject(wrong,key,encrypted.second,iv);
    wrong=encrypted.second;wrong[0]^=1;reject(encrypted.first,key,wrong,iv);
    std::printf("authenticated decryption checks=%u failed=%u\n",checks,failed);
    return failed?1:0;
}
