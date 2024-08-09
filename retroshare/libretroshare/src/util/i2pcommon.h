#ifndef I2PCOMMON_H
#define I2PCOMMON_H

#include <algorithm>
#include <map>
#include <array>
#include <utility>

#include "util/rsrandom.h"
#include "util/radix32.h"
#include "util/rsbase64.h"
#include "util/rsprint.h"
#include "util/rsdebug.h"

/*
 * This header provides common code for i2p related code, namely BOB and SAM3 support.
 */

namespace i2p {

static constexpr int8_t kDefaultLength   = 3; // i2p default
static constexpr int8_t kDefaultQuantity = 3; // i2p default + 1
static constexpr int8_t kDefaultVariance = 0;
static constexpr int8_t kDefaultBackupQuantity = 0;

/**
 * @brief The address struct
 * This structure is a container for any i2p address/key. The public key is used for addressing and can be (optionally) hashed to generate the .b32.i2p address.
 */
struct address {
	std::string base32;
	std::string publicKey;
	std::string privateKey;

	void clear() {
		base32.clear();
		publicKey.clear();
		privateKey.clear();
	}
};

/**
 * @brief The settings struct
 * Common structure with all settings that are shared between any i2p backends
 */
struct settings {
	bool enable;
	struct address address;

	// connection parameter
	int8_t inLength;
	int8_t inQuantity;
	int8_t inVariance;
	int8_t inBackupQuantity;

	int8_t outLength;
	int8_t outQuantity;
	int8_t outVariance;
	int8_t outBackupQuantity;

	void initDefault() {
		enable = false;
		address.clear();

		inLength    = kDefaultLength;
		inQuantity  = kDefaultQuantity;
		inVariance  = kDefaultVariance;
		inBackupQuantity = kDefaultBackupQuantity;

		outLength   = kDefaultLength;
		outQuantity = kDefaultQuantity;
		outVariance = kDefaultVariance;
		outBackupQuantity = kDefaultBackupQuantity;
	}
};

/*
	Type		Type Code	Payload Length	Total Length	Notes
	Null		0			0				3
	HashCash	1			varies			varies			Experimental, unused. Payload contains an ASCII colon-separated hashcash string.
	Hidden		2			0				3				Experimental, unused. Hidden routers generally do not announce that they are hidden.
	Signed		3			40 or 72		43 or 75		Experimental, unused. Payload contains a 40-byte DSA signature, optionally followed by the 32-byte Hash of the signing Destination.
	Multiple	4			varies			varies			Experimental, unused. Payload contains multiple certificates.
	Key			5			4+				7+				Since 0.9.12. See below for details.
*/
enum class CertType : uint8_t {
	Null     = 0,
	HashCash = 1,
	Hidden   = 2,
	Signed   = 3,
	Multiple = 4,
	Key      = 5
};

/*
 * public
	Type					Type Code	Total Public Key Length	Since	Usage
	DSA_SHA1				0			128						0.9.12	Legacy Router Identities and Destinations, never explicitly set
	ECDSA_SHA256_P256		1			64						0.9.12	Older Destinations
	ECDSA_SHA384_P384		2			96						0.9.12	Rarely if ever used for Destinations
	ECDSA_SHA512_P521		3			132						0.9.12	Rarely if ever used for Destinations
	RSA_SHA256_2048			4			256						0.9.12	Offline only; never used in Key Certificates for Router Identities or Destinations
	RSA_SHA384_3072			5			384						0.9.12	Offline only; never used in Key Certificates for Router Identities or Destinations
	RSA_SHA512_4096			6			512						0.9.12	Offline only; never used in Key Certificates for Router Identities or Destinations
	EdDSA_SHA512_Ed25519	7			32						0.9.15	Recent Router Identities and Destinations
	EdDSA_SHA512_Ed25519ph	8			32						0.9.25	Offline only; never used in Key Certificates for Router Identities or Destinations
	reserved (GOST)			9			64								Reserved, see proposal 134
	reserved (GOST)			10			128								Reserved, see proposal 134
	RedDSA_SHA512_Ed25519	11			32						0.9.39	For Destinations and encrypted leasesets only; never used for Router Identities
	reserved				65280-65534									Reserved for experimental use
	reserved				65535										Reserved for future expansion

 * private
	Type					Length (bytes)	Since	Usage
	DSA_SHA1				20						Legacy Router Identities and Destinations
	ECDSA_SHA256_P256		32				0.9.12	Recent Destinations
	ECDSA_SHA384_P384		48				0.9.12	Rarely used for Destinations
	ECDSA_SHA512_P521		66				0.9.12	Rarely used for Destinations
	RSA_SHA256_2048			512				0.9.12	Offline signing, never used for Router Identities or Destinations
	RSA_SHA384_3072			768				0.9.12	Offline signing, never used for Router Identities or Destinations
	RSA_SHA512_4096			1024			0.9.12	Offline signing, never used for Router Identities or Destinations
	EdDSA_SHA512_Ed25519	32				0.9.15	Recent Router Identities and Destinations
	EdDSA_SHA512_Ed25519ph	32				0.9.25	Offline signing, never used for Router Identities or Destinations
	RedDSA_SHA512_Ed25519	32				0.9.39	For Destinations and encrypted leasesets only, never used for Router Identities
 */
enum class SigningKeyType :  uint16_t {
	DSA_SHA1          = 0,
	ECDSA_SHA256_P256 = 1,
	ECDSA_SHA384_P384 = 2,
	ECDSA_SHA512_P521 = 3,
	RSA_SHA256_2048   = 4,
	RSA_SHA384_3072   = 5,
	RSA_SHA512_4096   = 6,
	EdDSA_SHA512_Ed25519   = 7,
	EdDSA_SHA512_Ed25519ph = 8,
	RedDSA_SHA512_Ed25519  = 11
};

/*
 * public
	Type		Type Code	Total Public Key Length	Usage
	ElGamal		0			256						All Router Identities and Destinations
	P256		1			64						Reserved, see proposal 145
	P384		2			96						Reserved, see proposal 145
	P521		3			132						Reserved, see proposal 145
	X25519		4			32						Not for use in key certs. See proposal 144
	reserved	65280-65534	 						Reserved for experimental use
	reserved	65535								Reserved for future expansion

 * private
	Type	Length (bytes)	Since	Usage
	ElGamal	256						All Router Identities and Destinations
	P256	32				TBD		Reserved, see proposal 145
	P384	48				TBD		Reserved, see proposal 145
	P521	66				TBD		Reserved, see proposal 145
	X25519	32				0.9.38	Little-endian. See proposal 144
*/
enum class CryptoKeyType : uint16_t {
	ElGamal = 0,
	P256    = 1,
	P384    = 2,
	P521    = 3,
	X25519  = 4
};

static const std::array<std::pair<uint16_t, uint16_t>, 5> cryptoKeyLengths {
	/*CryptoKeyType::ElGamal*/ std::make_pair<uint16_t, uint16_t>(256, 256),
	/*CryptoKeyType::P256,  */ std::make_pair<uint16_t, uint16_t>( 64,  32),
	/*CryptoKeyType::P384,  */ std::make_pair<uint16_t, uint16_t>( 96,  48),
	/*CryptoKeyType::P521,  */ std::make_pair<uint16_t, uint16_t>(132,  66),
	/*CryptoKeyType::X25519,*/ std::make_pair<uint16_t, uint16_t>( 32,  32),
};

static const std::array<std::pair<uint16_t, uint16_t>, 12> signingKeyLengths {
	/*SigningKeyType::DSA_SHA1,              */ std::make_pair<uint16_t, uint16_t>(128, 128),
	/*SigningKeyType::ECDSA_SHA256_P256,     */ std::make_pair<uint16_t, uint16_t>( 64,  32),
	/*SigningKeyType::ECDSA_SHA384_P384,     */ std::make_pair<uint16_t, uint16_t>( 96,  48),
	/*SigningKeyType::ECDSA_SHA512_P521,     */ std::make_pair<uint16_t, uint16_t>(132,  66),
	/*SigningKeyType::RSA_SHA256_2048,       */ std::make_pair<uint16_t, uint16_t>(256, 512),
	/*SigningKeyType::RSA_SHA384_3072,       */ std::make_pair<uint16_t, uint16_t>(384, 768),
	/*SigningKeyType::RSA_SHA512_4096,       */ std::make_pair<uint16_t, uint16_t>(512,1024),
	/*SigningKeyType::EdDSA_SHA512_Ed25519   */ std::make_pair<uint16_t, uint16_t>( 32,  32),
	/*SigningKeyType::EdDSA_SHA512_Ed25519ph */ std::make_pair<uint16_t, uint16_t>( 32,  32),
	/*reserved (GOST)                        */ std::make_pair<uint16_t, uint16_t>( 64,   0),
	/*reserved (GOST)                        */ std::make_pair<uint16_t, uint16_t>(128,   0),
	/*SigningKeyType::RedDSA_SHA512_Ed25519  */ std::make_pair<uint16_t, uint16_t>( 32,  32),
};

/*
 * Key length infos:
 *
 * BOB private key
 * len b64: 884
 * len pln: 663
 *
 * BOB public key / destination
 * len b64: 516
 * len pln: 387
 *
 * SAMv3 private key
 * len b64: 908
 * len pln: 679
 *
 * SAMv3 public key
 * len b64: 516
 * len pln: 387
 *
 * Example:
 * in bytes, public key only
 *   384 (Key) + 3 (Null certificate) = 387 bytes
 *   384 (Key) + 7 (key  certificate) = 391 bytes
 *
 * in bytes public + private key
 *   384 (Key) + 3 (Null certificate) + 256 (ElGamal) + 20 (DSA_SHA1) = 663 bytes
 *   384 (Key) + 7 (key  certificate) + 256 (ElGamal) + 32 (EdDSA_SHA512_Ed25519) = 679 bytes
 */
constexpr size_t pubKeyMinLenth_b64 = 516;
constexpr size_t pubKeyMinLenth_bin = 387;
constexpr size_t privKeyMinLenth_b64 = 884;
constexpr size_t privKeyMinLenth_bin = 663;

/**
 * @brief makeOption Creates the string "lhs=rhs" used by BOB and SAM. Converts rhs
 * @param lhs option to set
 * @param rhs value to set
 * @return concatenated string
 */
const std::string makeOption(const std::string &lhs, const int8_t &rhs);

/**
 * @brief keyToBase32Addr generated a base32 address (.b32.i2p) from a given public key
 * @param key public key
 * @return generated base32 address
 */
std::string keyToBase32Addr(const std::string &key);

/**
 * @brief publicKeyFromPrivate parses the private key and calculates the lenght of the public key
 * @param priv private key (which includes the public key) to read
 * @return public key used for addressing
 */
std::string  publicKeyFromPrivate(const std::string &priv);

/**
 * @brief getKeyTypes returns the name of the utilized algorithms used by the key
 * @param key public key
 * @param signingKey name of the signing key, e.g. DSA_SHA1
 * @param cryptoKey name of the crpyto key, e.g. ElGamal
 * @return true on success, false otherwise
 */
bool getKeyTypes(const std::string &key, std::string &signingKey, std::string &cryptoKey);

} // namespace i2p

#endif // I2PCOMMON_H
