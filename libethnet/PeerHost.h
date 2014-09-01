/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file EthereumHost.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <mutex>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include <utility>
#include <thread>
#include <libethential/Guards.h>
#include <libethcore/CommonEth.h>
#include "Common.h"
namespace ba = boost::asio;
namespace bi = boost::asio::ip;

namespace eth
{

class RLPStream;
class TransactionQueue;
class BlockQueue;

/**
 * @brief The PeerHost class
 * Capabilities should be registered prior to startNetwork, since m_capabilities is not thread-safe.
 */
class PeerHost
{
	friend class PeerSession;
	friend class HostCapabilityFace;

public:
	/// Start server, listening for connections on the given port.
	PeerHost(std::string const& _clientVersion, unsigned short _port, std::string const& _publicAddress = std::string(), bool _upnp = true);
	/// Start server, listening for connections on a system-assigned port.
	PeerHost(std::string const& _clientVersion, std::string const& _publicAddress = std::string(), bool _upnp = true);
	/// Start server, but don't listen.
	PeerHost(std::string const& _clientVersion);

	/// Will block on network process events.
	virtual ~PeerHost();

	/// Closes all peers.
	void disconnectPeers();

	/// Basic peer network protocol version.
	unsigned protocolVersion() const;

	/// Register a peer-capability; all new peer connections will have this capability.
	template <class T> void registerCapability(T* _t) { _t->m_host = this; m_capabilities[T::name()] = std::shared_ptr<HostCapabilityFace>(_t); }

	/// Connect to a peer explicitly.
	void connect(std::string const& _addr, unsigned short _port = 30303) noexcept;
	void connect(bi::tcp::endpoint const& _ep);

	/// Conduct I/O, polling, syncing, whatever.
	/// Ideally all time-consuming I/O is done in a background thread or otherwise asynchronously, but you get this call every 100ms or so anyway.
	/// This won't touch alter the blockchain.
	void process();

	/// @returns true iff we have the a peer of the given id.
	bool havePeer(Public _id) const;

	/// Set ideal number of peers.
	void setIdealPeerCount(unsigned _n) { m_idealPeerCount = _n; }

	/// Get peer information.
	std::vector<PeerInfo> peers(bool _updatePing = false) const;

	/// Get number of peers connected; equivalent to, but faster than, peers().size().
	size_t peerCount() const { Guard l(x_peers); return m_peers.size(); }

	/// Ping the peers, to update the latency information.
	void pingAll();

	/// Get the port we're listening on currently.
	unsigned short listenPort() const { return m_public.port(); }

	/// Serialise the set of known peers.
	bytes savePeers() const;

	/// Deserialise the data and populate the set of known peers.
	void restorePeers(bytesConstRef _b);

	void registerPeer(std::shared_ptr<PeerSession> _s, std::vector<std::string> const& _caps);

	bool haveCapability(std::string const& _name) const { return m_capabilities.count(_name); }
	std::vector<std::string> caps() const { std::vector<std::string> ret; for (auto const& i: m_capabilities) ret.push_back(i.first); return ret; }

protected:
	/// Called when the session has provided us with a new peer we can connect to.
	void noteNewPeers() {}

	void seal(bytes& _b);
	void populateAddresses();
	void determinePublic(std::string const& _publicAddress, bool _upnp);
	void ensureAccepting();

	void growPeers();
	void prunePeers();

	std::map<Public, bi::tcp::endpoint> potentialPeers();

	std::string m_clientVersion;

	unsigned short m_listenPort;

	ba::io_service m_ioService;
	bi::tcp::acceptor m_acceptor;
	bi::tcp::socket m_socket;

	UPnP* m_upnp = nullptr;
	bi::tcp::endpoint m_public;
	KeyPair m_key;

	mutable std::mutex x_peers;
	mutable std::map<Public, std::weak_ptr<PeerSession>> m_peers;	// mutable because we flush zombie entries (null-weakptrs) as regular maintenance from a const method.

	std::map<Public, std::pair<bi::tcp::endpoint, unsigned>> m_incomingPeers;	// TODO: does this need a lock?
	std::vector<Public> m_freePeers;

	std::chrono::steady_clock::time_point m_lastPeersRequest;
	unsigned m_idealPeerCount = 5;

	std::vector<bi::address_v4> m_addresses;
	std::vector<bi::address_v4> m_peerAddresses;

	std::map<std::string, std::shared_ptr<HostCapabilityFace>> m_capabilities;

	bool m_accepting = false;
};

}
