// Copyright (c) 2018 Duality Blockchain Solutions Developers
// TODO: Add License

#ifndef DYNAMIC_DHT_STORAGE_H
#define DYNAMIC_DHT_STORAGE_H

#include <libtorrent/kademlia/dht_storage.hpp>
#include <libtorrent/kademlia/dht_settings.hpp>

using namespace libtorrent;
using namespace libtorrent::dht;

class CDHTStorage final : public dht_storage_interface
{
public:

    explicit CDHTStorage(dht_settings const& settings)
        : m_settings(settings)
    {
        pDefaultStorage = dht_default_storage_constructor(settings);
    }

    ~CDHTStorage() override = default;

    CDHTStorage(CDHTStorage const&) = delete;
    CDHTStorage& operator=(CDHTStorage const&) = delete;

    size_t num_torrents() const override;
    size_t num_peers() const override;
    void update_node_ids(std::vector<sha1_hash> const& ids) override;
    bool get_peers(sha1_hash const& info_hash, bool const noseed, bool const scrape, address const& requester, entry& peers) const override;
    void announce_peer(sha1_hash const& info_hash, tcp::endpoint const& endp, string_view name, bool const seed) override;
    // Do not support get immutable item
    bool get_immutable_item(sha1_hash const& target, entry& item) const override;
    // Do not support put immutable item   
    void put_immutable_item(sha1_hash const& target, span<char const> buf, address const& addr) override;
    bool get_mutable_item_seq(sha1_hash const& target, sequence_number& seq) const override;
    bool get_mutable_item(sha1_hash const& target, sequence_number const seq, bool const force_fill, entry& item) const override;
    void put_mutable_item(sha1_hash const& target
        , span<char const> buf
        , signature const& sig
        , sequence_number const seq
        , public_key const& pk
        , span<char const> salt
        , address const& addr) override;
    
    int get_infohashes_sample(entry& item) override;
    void tick() override;
    dht_storage_counters counters() const override;

private:
    std::unique_ptr<dht_storage_interface> pDefaultStorage;
    dht_settings const& m_settings;

};

std::unique_ptr<dht_storage_interface> CDHTStorageConstructor(dht_settings const& settings);
void ExtractValue(std::unique_ptr<char[]>& value, const span<char const>& buf);

#endif // DYNAMIC_DHT_STORAGE_H
