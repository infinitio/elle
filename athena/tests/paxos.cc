#include <elle/serialization/binary.hh>
#include <elle/serialization/json.hh>
#include <elle/test.hh>

#include <reactor/Barrier.hh>
#include <reactor/scheduler.hh>
#include <reactor/signal.hh>

#include <athena/paxos/Client.hh>
#include <athena/paxos/Server.hh>

ELLE_LOG_COMPONENT("athena.paxos.test");

namespace paxos = athena::paxos;

namespace std
{
  std::ostream&
  operator <<(std::ostream& output, boost::optional<int> const& v);

  std::ostream&
  operator <<(std::ostream& output, boost::optional<int> const& v)
  {
    if (v)
      elle::fprintf(output, "Some(%s)", v.get());
    else
      elle::fprintf(output, "None");
    return output;
  }
}

template <typename T, typename Version, typename ServerId>
class Peer
  : public paxos::Client<T, Version, ServerId>::Peer
{
public:
  typedef paxos::Client<T, Version, ServerId> Client;

  Peer(ServerId id,
       paxos::Server<T, Version, ServerId>& paxos)
    : paxos::Client<T, Version, ServerId>::Peer(id)
    , _paxos(paxos)
  {}

  virtual
  boost::optional<typename Client::Accepted>
  propose(
    typename paxos::Server<T, Version, ServerId>::Quorum const& q,
    typename Client::Proposal const& p) override
  {
    return this->_paxos.propose(q, p);
  }

  virtual
  typename Client::Proposal
  accept(
    typename paxos::Server<T, Version, ServerId>::Quorum const& q,
    typename Client::Proposal const& p,
    elle::Option<T, typename Client::Quorum> const& value) override
  {
    return this->_paxos.accept(q, p, value);
  }

  virtual
  void
  confirm(
    typename paxos::Server<T, Version, ServerId>::Quorum const& q,
    typename Client::Proposal const& p) override
  {
    return this->_paxos.confirm(q, p);
  }

  virtual
  boost::optional<typename Client::Accepted>
  get(
    typename paxos::Server<T, Version, ServerId>::Quorum const& q) override
  {
    return this->_paxos.get(q);
  }

  ELLE_ATTRIBUTE_RW((paxos::Server<T, Version, ServerId>&), paxos);
};

ELLE_TEST_SCHEDULED(all_is_well)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers;
  peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client(1, std::move(peers));
  try
  {
    BOOST_CHECK(!client.choose(42));
  }
  catch (paxos::Server<int, int, int>::WrongQuorum const& q)
  {
    BOOST_CHECK_EQUAL(q.effective(), q.expected());
    throw;
  }
  BOOST_CHECK_EQUAL(client.get(), 42);
}

template <typename T, typename Version, typename ServerId>
class UnavailablePeer
  : public paxos::Client<T, Version, ServerId>::Peer
{
public:
  typedef paxos::Client<T, Version, ServerId> Client;

  UnavailablePeer(ServerId id)
    : paxos::Client<T, Version, ServerId>::Peer(std::move(id))
  {}

  virtual
  boost::optional<typename Client::Accepted>
  propose(typename Client::Quorum const& q,
          typename Client::Proposal const& p) override
  {
    throw typename Client::Peer::Unavailable();
  }

  virtual
  typename Client::Proposal
  accept(typename Client::Quorum const& q,
         typename Client::Proposal const& p,
         elle::Option<T, typename Client::Quorum> const& value) override
  {
    throw typename Client::Peer::Unavailable();
  }

  virtual
  void
  confirm(typename Client::Quorum const& q,
          typename Client::Proposal const& p) override
  {
    throw typename Client::Peer::Unavailable();
  }

  virtual
  boost::optional<typename Client::Accepted>
  get(typename Client::Quorum const& q) override
  {
    throw typename Client::Peer::Unavailable();
  }
};

ELLE_TEST_SCHEDULED(two_of_three)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers;
  peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(13));
  paxos::Client<int, int, int> client(1, std::move(peers));
  BOOST_CHECK(!client.choose(42));
  BOOST_CHECK_EQUAL(client.get(), 42);
}

ELLE_TEST_SCHEDULED(one_of_three)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers;
  peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(12));
  peers.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(13));
  paxos::Client<int, int, int> client(1, std::move(peers));
  BOOST_CHECK_THROW(client.choose(42), athena::paxos::TooFewPeers);
  BOOST_CHECK_THROW(client.get(), athena::paxos::TooFewPeers);
}

ELLE_TEST_SCHEDULED(already_chosen)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  Peers peers_2;
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_2(1, std::move(peers_2));
  BOOST_CHECK(!client_1.choose(42));
  auto chosen = client_2.choose(43);
  BOOST_CHECK(chosen);
  BOOST_CHECK(chosen->value.is<int>());
  BOOST_CHECK_EQUAL(chosen->value.get<int>(), 42);
}

template <typename T, typename Version, typename ServerId>
class InstrumentedPeer
  : public Peer<T, Version, ServerId>
{
public:
  typedef paxos::Client<T, Version, ServerId> Client;

  InstrumentedPeer(ServerId id, paxos::Server<T, Version, ServerId>& paxos)
    : Peer<T, Version, ServerId>(id, paxos)
    , fail(false)
  {}

  ~InstrumentedPeer() noexcept(true)
  {}

  virtual
  boost::optional<typename Client::Accepted>
  propose(
    typename Client::Quorum const& q,
    typename Client::Proposal const& p) override
  {
    if (fail)
      throw typename Peer<T, Version, ServerId>::Unavailable();
    this->propose_signal.signal();
    reactor::wait(this->propose_barrier);
    return Peer<T, Version, ServerId>::propose(q, p);
  }

  virtual
  typename Client::Proposal
  accept(typename Client::Quorum const& q,
         typename Client::Proposal const& p,
         elle::Option<T, typename Client::Quorum> const& value) override
  {
    if (fail)
      throw typename Peer<T, Version, ServerId>::Unavailable();
    this->accept_signal.signal();
    reactor::wait(this->accept_barrier);
    return Peer<T, Version, ServerId>::accept(q, p, value);
  }

  bool fail;
  reactor::Barrier propose_barrier, accept_barrier;
  reactor::Signal propose_signal, accept_signal;
};

ELLE_TEST_SCHEDULED(concurrent)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  auto peer_1_2 = new InstrumentedPeer<int, int, int>(12, server_2);
  auto peer_1_3 = new InstrumentedPeer<int, int, int>(13, server_3);
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_1.push_back(std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_2));
  peers_1.push_back(std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_3));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  Peers peers_2;
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_2(2, std::move(peers_2));
  peer_1_2->propose_barrier.open();
  peer_1_2->accept_barrier.open();
  peer_1_3->propose_barrier.open();
  reactor::Thread::unique_ptr t1(
    new reactor::Thread(
      "1",
      [&]
      {
        auto chosen = client_1.choose(42);
        BOOST_CHECK_EQUAL(chosen->value.get<int>(), 42);
      }));
  reactor::wait(
    reactor::Waitables({&peer_1_2->accept_signal, &peer_1_3->accept_signal}));
  auto chosen = client_2.choose(43);
  BOOST_CHECK_EQUAL(chosen->value.get<int>(), 42);
  peer_1_3->accept_barrier.open();
  reactor::wait(*t1);
}

ELLE_TEST_SCHEDULED(conflict)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  auto peer_1_2 = new InstrumentedPeer<int, int, int>(12, server_2);
  auto peer_1_3 = new InstrumentedPeer<int, int, int>(13, server_3);
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_1.push_back(
    std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_2));
  peers_1.push_back(
    std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_3));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  Peers peers_2;
  peers_2.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(11));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_2(2, std::move(peers_2));
  peer_1_2->propose_barrier.open();
  peer_1_3->propose_barrier.open();
  reactor::Thread::unique_ptr t1(
    new reactor::Thread(
      "1",
      [&]
      {
        auto chosen = client_1.choose(43);
        BOOST_CHECK_EQUAL(chosen->value.get<int>(), 42);
      }));
  reactor::wait(
    reactor::Waitables({&peer_1_2->accept_signal, &peer_1_3->accept_signal}));
  BOOST_CHECK(!client_2.choose(42));
  peer_1_2->accept_barrier.open();
  peer_1_3->accept_barrier.open();
  reactor::wait(*t1);
}

// Check a newer version overrides a previous result
ELLE_TEST_SCHEDULED(versions)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  Peers peers_2;
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_2(1, std::move(peers_2));
  ELLE_LOG("choose 1 for version 1")
    BOOST_CHECK(!client_1.choose(1, 1));
  ELLE_LOG("choose 2 for version 2")
    BOOST_CHECK(!client_2.choose(2, 2));
}

// Check a newer version, even partially agreed on, is taken in account
ELLE_TEST_SCHEDULED(versions_partial)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  auto peer_1_1 = new InstrumentedPeer<int, int, int>(11, server_1);
  peers_1.push_back(
    std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_1));
  peer_1_1->propose_barrier.open();
  peer_1_1->accept_barrier.open();
  auto peer_1_2 = new InstrumentedPeer<int, int, int>(12, server_2);
  peer_1_2->propose_barrier.open();
  peers_1.push_back(
    std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_2));
  auto peer_1_3 = new InstrumentedPeer<int, int, int>(13, server_3);
  peer_1_3->propose_barrier.open();
  peers_1.push_back(
    std::unique_ptr<paxos::Client<int, int, int>::Peer>(peer_1_3));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  ELLE_LOG("select 1 for version 1 (client 2)")
  {
    Peers peers_2;
    peers_2.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
    peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
    peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
    paxos::Client<int, int, int> client_2(2, std::move(peers_2));
    BOOST_CHECK(!client_2.choose(1, 1));
  }
  //        11        12        13
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:2  |  1:1:2  |  1:1:2  |
  //   +---------+---------+---------+
  reactor::Thread::unique_ptr t(
    new reactor::Thread("client_1",
                        [&]
                        {
                          auto chosen = client_1.choose(2, 2);
                          BOOST_CHECK(chosen);
                          BOOST_CHECK_EQUAL(chosen->value.get<int>(), 2);
                        }));
  reactor::wait(peer_1_1->accept_signal);
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |    2    |         |         | -> ?
  //   |  2:1:1  |  2:1:1  |  2:1:1  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:2  |  1:1:2  |  1:1:2  |
  //   +---------+---------+---------+
  ELLE_LOG("select 1 for version 1 (client 3)")
  {
    Peers peers_3;
    peers_3.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
    peers_3.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
    peers_3.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
    paxos::Client<int, int, int> client_3(3, std::move(peers_3));
    auto chosen = client_3.choose(1, 1);
    BOOST_CHECK_EQUAL(chosen->value.get<int>(), 2);
  }
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |    2    |    2    |    2    | -> 2
  //   |  2:2:3  |  2:2:3  |  2:2:3  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:2  |  1:1:2  |  1:1:2  |
  //   +---------+---------+---------+
  peer_1_2->accept_barrier.open();
  peer_1_3->accept_barrier.open();
  reactor::wait(*t);
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |    2    |    2    |    2    | -> 2
  //   |  2:3:1  |  2:3:1  |  2:3:1  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:2  |  1:1:2  |  1:1:2  |
  //   +---------+---------+---------+
}

// Check a failed newer version doesn't block older ones
ELLE_TEST_SCHEDULED(versions_aborted)
{
  paxos::Server<int, int, int> server_1(11, {11, 12, 13});
  paxos::Server<int, int, int> server_2(12, {11, 12, 13});
  paxos::Server<int, int, int> server_3(13, {11, 12, 13});
  typedef paxos::Client<int, int, int>::Peers Peers;
  Peers peers_1;
  peers_1.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  peers_1.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(12));
  peers_1.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(13));
  paxos::Client<int, int, int> client_1(1, std::move(peers_1));
  Peers peers_2;
  peers_2.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(11));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
  peers_2.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
  paxos::Client<int, int, int> client_2(2, std::move(peers_2));
  BOOST_CHECK_THROW(client_1.choose(2, 2), elle::Error);
  BOOST_CHECK(!client_2.choose(1, 1));
}

ELLE_TEST_SCHEDULED(elect_extend)
{
  typedef paxos::Server<int, int, int> Server;
  typedef paxos::Client<int, int, int> Client;
  typedef Client::Peers Peers;
  Server server_1(11, {11});
  Server server_2(12, {11, 12});
  Peers peers;
  peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
  paxos::Client<int, int, int> client(1, std::move(peers));
  ELLE_LOG("choose 0 for version 0")
    BOOST_CHECK(!client.choose(0, 0));
  //        11
  //   +---------+
  // 0 |    0    | -> 1
  //   |  1:1:1  |
  //   +---------+
  ELLE_LOG("fail choosing 1 for version 0")
    BOOST_CHECK_EQUAL(client.choose(0, 1)->value.get<int>(), 0);
  ELLE_LOG("fail choosing quorum of 2 for version 0");
    BOOST_CHECK_EQUAL(
      client.choose(0, Client::Quorum{11, 12})->value.get<int>(), 0);
  ELLE_LOG("choose quorum of 2 for version 1")
    BOOST_CHECK(!client.choose(1, Client::Quorum{11, 12}));
  //        11
  //   +---------+
  // 1 | {11,12} | -> {11,12}
  //   |  1:1:1  |
  //   +---------+
  // 0 |    0    | -> 1
  //   |  0:1:1  |
  //   +---------+
  ELLE_LOG("fail choosing 1 for version 1")
    BOOST_CHECK_EQUAL(client.choose(1, 1)->value.get<Client::Quorum>(),
                      Client::Quorum({11, 12}));
  ELLE_LOG("fail choosing 2 for version 2 with partial quorum")
    BOOST_CHECK_THROW(client.choose(2, 2), Server::WrongQuorum);
  client.peers().emplace_back(
    elle::make_unique<Peer<int, int, int>>(12, server_2));
  ELLE_LOG("choose 2 for version 2 with extended quorum")
    BOOST_CHECK(!client.choose(2, 2));
  //        11
  //   +---------+---------+
  // 2 |    2    |    2    | -> 2
  //   |  2:1:1  |  2:1:1  |
  //   +---------+---------+
  // 1 | {11,12} |           -> {11,12}
  //   |  1:1:1  |
  //   +---------+
  // 0 |    0    |           -> 1
  //   |  0:1:1  |
  //   +---------+
  ELLE_LOG("choose 3 for version 3 with extended quorum")
    BOOST_CHECK(!client.choose(3, 3));
  //        11
  //   +---------+---------+
  // 3 |    3    |    3    | -> 3
  //   |  3:1:1  |  3:1:1  |
  //   +---------+---------+
  // 2 |    2    |    2    | -> 2
  //   |  2:1:1  |  3:1:1  |
  //   +---------+---------+
  // 1 | {11,12} |           -> {11,12}
  //   |  1:1:1  |
  //   +---------+
  // 0 |    0    |           -> 1
  //   |  0:1:1  |
  //   +---------+
}

ELLE_TEST_SCHEDULED(elect_shrink)
{
  typedef paxos::Server<int, int, int> Server;
  typedef paxos::Client<int, int, int> Client;
  typedef Peer<int, int, int> Peer;
  typedef Client::Peers Peers;
  Server server_1(11, {11, 12});
  Server server_2(12, {11, 12});
  Peers peers;
  peers.push_back(elle::make_unique<Peer>(11, server_1));
  peers.push_back(elle::make_unique<Peer>(12, server_2));
  paxos::Client<int, int, int> client(1, std::move(peers));
  ELLE_LOG("choose initial value")
    BOOST_CHECK(!client.choose(0, 0));
  ELLE_LOG("choose quorum of 1")
    BOOST_CHECK(!client.choose(1, Client::Quorum{11}));
  ELLE_LOG("fail choosing with superfluous server")
    BOOST_CHECK_THROW(client.choose(2, 2), Server::WrongQuorum);
  ELLE_LOG("drop peer 2 and choose")
  {
    client.peers().pop_back();
    BOOST_CHECK(!client.choose(2, 2));
  }
}

ELLE_TEST_SCHEDULED(evict_down_lag_behind)
{
  typedef paxos::Server<int, int, int> Server;
  typedef paxos::Client<int, int, int> Client;
  typedef Peer<int, int, int> Peer;
  typedef Client::Peers Peers;
  Server server_1(11, {11, 12, 13});
  Server server_2(12, {11, 12, 13});
  Server server_3(13, {11, 12, 13});
  auto make_client = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<Peer>(11, server_1));
      peers.push_back(elle::make_unique<Peer>(12, server_2));
      peers.push_back(elle::make_unique<Peer>(13, server_3));
      return paxos::Client<int, int, int>(1, std::move(peers));
    };
  auto make_2client = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<Peer>(11, server_1));
      peers.push_back(elle::make_unique<Peer>(12, server_2));
      return paxos::Client<int, int, int>(1, std::move(peers));
    };
  auto make_partial_client = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<Peer>(11, server_1));
      peers.push_back(elle::make_unique<Peer>(12, server_2));
      peers.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(13));
      return paxos::Client<int, int, int>(1, std::move(peers));
    };
  ELLE_LOG("choose 1 for version 1")
    BOOST_CHECK(!make_client().choose(1, 1));
  ELLE_LOG("choose 2 for version 2")
    BOOST_CHECK(!make_client().choose(2, 2));
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |    2    |    2    |    2    | -> 2
  //   |  2:1:1  |  2:1:1  |  2:1:1  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:1  |  1:1:1  |  1:1:1  |
  //   +---------+---------+---------+
  ELLE_LOG("choose quorum {11, 12} for version 3")
    BOOST_CHECK(!make_partial_client().choose(3, Client::Quorum{11, 12}));
  //        11        12        13
  //   +---------+---------+---------+
  // 3 | {11,12} | {11,12} |         | -> {11,12}
  //   |  3:1:1  |  3:1:1  |         |
  //   +---------+---------+---------+
  // 2 |    2    |    2    |    2    | -> 2
  //   |  2:1:1  |  2:1:1  |  2:1:1  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |    1    | -> 1
  //   |  1:1:1  |  1:1:1  |  1:1:1  |
  //   +---------+---------+---------+
  ELLE_LOG("choose 21 for version 2")
    // Both exception and response are deemed valid behavior
    try
    {
      auto res = make_2client().choose(2, 21);
      BOOST_CHECK(res);
      BOOST_CHECK(res->confirmed);
      BOOST_CHECK(res->value.template is<Client::Quorum>());
      BOOST_CHECK_EQUAL(res->value.template get<Client::Quorum>(),
                        (Client::Quorum{11, 12}));
    }
    catch (Server::WrongQuorum const& e)
    {
      BOOST_CHECK_EQUAL(e.effective(), (Client::Quorum{11, 12}));
      BOOST_CHECK_EQUAL(e.expected(), (Client::Quorum{11, 12, 13}));
    }
  ELLE_LOG("choose 21 for version 2")
  {
    auto chosen = make_partial_client().choose(2, 21);
    BOOST_CHECK(chosen);
    BOOST_CHECK_EQUAL(chosen->value.get<Client::Quorum>(),
                      (Client::Quorum{11, 12}));
    BOOST_CHECK_EQUAL(chosen->proposal.version, 3);
  }
  ELLE_LOG("choose 4 for version 4")
    BOOST_CHECK(!make_2client().choose(4, 4));
}

template <typename T, typename Version, typename ServerId>
class ProposeOnlyPeer
  : public Peer<T, Version, ServerId>
{
public:
  typedef paxos::Client<T, Version, ServerId> Client;
  typedef Peer<T, Version, ServerId> Super;

  ProposeOnlyPeer(ServerId id, paxos::Server<T, Version, ServerId>& paxos)
    : Peer<T, Version, ServerId>(id, paxos)
  {}

  virtual
  typename Client::Proposal
  accept(typename Client::Quorum const& q,
         typename Client::Proposal const& p,
         elle::Option<T, typename Client::Quorum> const& value) override
  {
    throw typename Super::Unavailable();
  }

  bool fail;
  reactor::Barrier propose_barrier, accept_barrier;
  reactor::Signal propose_signal, accept_signal;
};

namespace quorum_divergence
{
  ELLE_TEST_SCHEDULED(one_of_three_thinks_quorum_changed)
  {
    typedef paxos::Server<int, int, int> Server;
    typedef paxos::Client<int, int, int> Client;
    typedef Client::Peers Peers;
    Server server_1(11, {11, 12, 13});
    Server server_2(12, {11, 12, 13});
    Server server_3(13, {11, 12, 13});
    auto client = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
      peers.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
      peers.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
      return paxos::Client<int, int, int>(1, std::move(peers));
    }();
    auto client_1_only = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<Peer<int, int, int>>(11, server_1));
      peers.push_back(
        elle::make_unique<ProposeOnlyPeer<int, int, int>>(12, server_2));
      peers.push_back(
        elle::make_unique<ProposeOnlyPeer<int, int, int>>(13, server_3));
      return paxos::Client<int, int, int>(2, std::move(peers));
    }();
    auto client_partial_23 = [&]
    {
      Peers peers;
      peers.push_back(elle::make_unique<UnavailablePeer<int, int, int>>(11));
      peers.push_back(elle::make_unique<Peer<int, int, int>>(12, server_2));
      peers.push_back(elle::make_unique<Peer<int, int, int>>(13, server_3));
      return paxos::Client<int, int, int>(3, std::move(peers));
    }();
    ELLE_LOG("choose 1 for version 1")
      BOOST_CHECK(!client.choose(1, 1));
    //        11        12        13
    //   +---------+---------+---------+
    // 1 |    1    |    1    |    1    | -> 1
    //   |  1:1:1  |  1:1:1  |  1:1:1  |
    //   +---------+---------+---------+
    ELLE_LOG("choose quorum {11,12} for version 2 on server 11 only")
      BOOST_CHECK_THROW(client_1_only.choose(2, Client::Quorum{11, 12}),
                        athena::paxos::TooFewPeers);
    //        11        12        13
    //   +---------+---------+---------+
    // 2 | {11,12} |         |         |
    //   |  2:1:2  |  2:1:2  |  2:1:2  |
    //   +---------+---------+---------+
    // 1 |    1    |    1    |    1    | -> 1
    //   |  1:1:1  |  1:1:1  |  1:1:1  |
    //   +---------+---------+---------+
    ELLE_LOG("choose 2 for version 2")
      BOOST_CHECK(!client_partial_23.choose(2, 2));
    //        11        12        13
    //   +---------+---------+---------+
    // 2 | {11,12} |    2    |    2    | -> 2
    //   |  2:1:2  |  2:1:3  |  2:1:3  |
    //   +---------+---------+---------+
    // 1 |    1    |    1    |    1    | -> 1
    //   |  1:1:1  |  1:1:1  |  1:1:1  |
    //   +---------+---------+---------+
    ELLE_LOG("choose 3 for version 3")
      BOOST_CHECK(!client.choose(3, 3));
  }
}

// FIXME: Doesn't test incomplete rounds serialization, like proposed but
// not-accepted rounds.
ELLE_TEST_SCHEDULED(serialization)
{
  typedef paxos::Server<int, int, int> Server;
  typedef paxos::Client<int, int, int> Client;
  typedef Peer<int, int, int> Peer;
  typedef Client::Peers Peers;
  elle::Buffer s1;
  elle::Buffer s2;
  ELLE_LOG("choose and serialize")
  {
    Server server_1(11, {11, 12});
    Server server_2(12, {11, 12});
    Peers peers;
    peers.emplace_back(new Peer(11, server_1));
    peers.emplace_back(new Peer(12, server_2));
    paxos::Client<int, int, int> client(1, std::move(peers));
    BOOST_CHECK(!client.choose(0, 0));
    BOOST_CHECK(!client.choose(1, 1));
    s1 = elle::serialization::json::serialize(server_1, false);
    s2 = elle::serialization::json::serialize(server_2, false);
  }
  ELLE_LOG("unserialize and choose")
  {
    auto server_1 = elle::serialization::json::deserialize<Server>(s1, false);
    auto server_2 = elle::serialization::json::deserialize<Server>(s2, false);
    Peers peers;
    peers.emplace_back(new Peer(11, server_1));
    peers.emplace_back(new Peer(12, server_2));
    paxos::Client<int, int, int> client(1, std::move(peers));
    BOOST_CHECK_EQUAL(client.choose(1, 0)->value.template get<int>(), 1);
    BOOST_CHECK(!client.choose(2, 2));
  }
}

ELLE_TEST_SCHEDULED(partial_state)
{
  typedef paxos::Server<int, int, int> Server;
  typedef paxos::Client<int, int, int> Client;
  typedef Peer<int, int, int> Peer;
  typedef Client::Peers Peers;
  std::vector<Server> servers
  {
    {11, {11, 12, 13}},
    {12, {11, 12, 13}},
    {13, {11, 12, 13}},
  };
  auto make_client = [&] (std::vector<bool> availability)
    {
      Peers peers;
      int i = 0;
      for (auto a: availability)
      {
        if (a)
          peers.emplace_back(new Peer(11 + i, servers[i]));
        else
          peers.emplace_back(new UnavailablePeer<int, int, int>(11 + i));
        ++i;
      }
      return paxos::Client<int, int, int>(1, std::move(peers));
    };
  ELLE_LOG("select 0 for version 0")
    BOOST_CHECK(!make_client({true, true, true}).choose(0, 0));
  ELLE_LOG("select 1 for version 1 on {11, 12}")
    BOOST_CHECK(!make_client({true, true, false}).choose(1, 1));
  ELLE_LOG("propose 2 for version 2 on {11}")
    BOOST_CHECK_THROW(make_client({true, false, false}).choose(2, 2),
                      athena::paxos::TooFewPeers);
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |         |         |         |
  //   |  2:1:1  |         |         |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |         | -> 1
  //   |  1:1:1  |  1:1:1  |         |
  //   +---------+---------+---------+
  // 0 |    0    |    0    |    0    | -> 0
  //   |  0:1:1  |  0:1:1  |  0:1:1  |
  //   +---------+---------+---------+
  ELLE_LOG("propose 2 for version 1 on {11, 13}")
    BOOST_CHECK(!make_client({true, false, true}).choose(1, 2));
  //        11        12        13
  //   +---------+---------+---------+
  // 2 |    2    |         |    2    | -> 2
  //   |  2:2:1  |         |  2:2:1  |
  //   +---------+---------+---------+
  // 1 |    1    |    1    |         | -> 1
  //   |  1:1:1  |  1:1:1  |         |
  //   +---------+---------+---------+
  // 0 |    0    |    0    |    0    | -> 0
  //   |  0:1:1  |  0:1:1  |  0:1:1  |
  //   +---------+---------+---------+
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(all_is_well), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(two_of_three), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(one_of_three), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(already_chosen), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(concurrent), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(conflict), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(versions), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(versions_partial), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(versions_aborted), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(serialization), 0, valgrind(1));
  suite.add(BOOST_TEST_CASE(partial_state), 0, valgrind(5));
  {
    auto quorum = BOOST_TEST_SUITE("quorum");
    suite.add(quorum);
    quorum->add(BOOST_TEST_CASE(elect_extend), 0, valgrind(1));
    quorum->add(BOOST_TEST_CASE(elect_shrink), 0, valgrind(1));
    quorum->add(BOOST_TEST_CASE(evict_down_lag_behind), 0, valgrind(1));
    {
      auto divergence = BOOST_TEST_SUITE("divergence");
      using namespace quorum_divergence;
      quorum->add(divergence);
      divergence->add(
        BOOST_TEST_CASE(one_of_three_thinks_quorum_changed), 0, valgrind(1));
    }
  }
}
