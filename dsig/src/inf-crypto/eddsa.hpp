#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <dory/memstore/store.hpp>
#include <dory/shared/logger.hpp>

#include "../config.hpp"
#include "../export/base-types.hpp"
#include "batch.hpp"

// Use Dalek or Sodium
#include <dory/crypto/asymmetric/dalek.hpp>
#define crypto_impl dory::crypto::asymmetric::dalek

// #include <dory/crypto/asymmetric/sodium.hpp>
// #define crypto_impl dory::crypto::asymmetric::sodium

namespace dory::dsig {
class EddsaCrypto {
 public:
  using Signature = std::array<uint8_t, crypto_impl::SignatureLength>;
  using BatchedSignature = Batched<Signature>;

  EddsaCrypto(ProcId local_id, std::vector<ProcId> const &all_ids)
      : my_id{local_id}, store{nspace}, LOGGER_INIT(logger, "Dsig") {
    crypto_impl::init();

    LOGGER_INFO(logger, "Publishing my EdDSA key (process {})", my_id);
    crypto_impl::publish_pub_key(fmt::format("{}-dsig-pubkey", local_id));

    LOGGER_INFO(logger, "Waiting for all processes ({}) to publish their keys",
                all_ids);
    store.barrier("dsig_public_keys_announced", all_ids.size());

    for (auto id : all_ids) {
      public_keys.emplace(
          id, crypto_impl::get_public_key(fmt::format("{}-dsig-pubkey", id)));
    }
  }

  inline Signature sign(uint8_t const *msg,      // NOLINT
                        size_t const msg_len) {  // NOLINT
    Signature sig;
    crypto_impl::sign(sig.data(), msg, msg_len);
    return sig;
  }

  inline bool verify(Signature const &sig, uint8_t const *msg,
                     size_t const msg_len, ProcId const node_id) {
    auto pk_it = public_keys.find(node_id);
    if (pk_it == public_keys.end()) {
      throw std::runtime_error(
          fmt::format("Missing public key for {}!", node_id));
    }

    return crypto_impl::verify(sig.data(), msg, msg_len, pk_it->second);
  }

  inline bool verify(BatchedSignature const &sig, ProcId const node_id) {
    auto const root = sig.proof.root(sig.signed_hash, sig.index);
    return verify(sig.root_sig, reinterpret_cast<uint8_t const*>(root.data()), sizeof(root), node_id);
  }

  inline ProcId myId() const { return my_id; }

 private:
  ProcId const my_id;
  memstore::MemoryStore store;

  // Map: NodeId (ProcId) -> Node's Public Key
  std::unordered_map<ProcId, crypto_impl::pub_key> public_keys;
  LOGGER_DECL(logger);
};

}  // namespace dory::dsig
