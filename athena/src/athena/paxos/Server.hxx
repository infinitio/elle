#ifndef ATHENA_PAXOS_SERVER_HXX
# define ATHENA_PAXOS_SERVER_HXX

# include <boost/multi_index_container.hpp>
# include <boost/multi_index/mem_fun.hpp>

# include <elle/With.hh>
# include <elle/serialization/Serializer.hh>

# include <reactor/Scope.hh>

namespace athena
{
  namespace paxos
  {
    /*---------.
    | Proposal |
    `---------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Proposal::Proposal(
      Version version_, int round_, ClientId sender_)
      : version(version_)
      , round(round_)
      , sender(sender_)
    {}

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Proposal::Proposal()
      : version()
      , round()
      , sender()
    {}

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    bool
    Server<T, Version, ClientId, ServerId>::Proposal::operator ==(
      Proposal const& rhs) const
    {
      return this->version == rhs.version &&
        this->round == rhs.round &&
        this->sender == rhs.sender;
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    bool
    Server<T, Version, ClientId, ServerId>::Proposal::operator <(
      Proposal const& rhs) const
    {
      if (this->version != rhs.version)
        return this->version < rhs.version;
      else if (this->round != rhs.round)
        return this->round < rhs.round;
      else
        return this->sender < rhs.sender;
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::Proposal::serialize(
      elle::serialization::Serializer& s)
    {
      s.serialize("version", this->version);
      s.serialize("round", this->round);
      s.serialize("sender", this->sender);
    }

    /*---------.
    | Accepted |
    `---------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Accepted::Accepted(
      Proposal proposal_,
      elle::Option<T, Quorum> value_,
      bool confirmed_)
      : proposal(std::move(proposal_))
      , value(std::move(value_))
      , confirmed(confirmed_)
    {}

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Accepted::Accepted(
      elle::serialization::SerializerIn& s, elle::Version const& v)
      : proposal()
      , value(Quorum())
      , confirmed(false)
    {
      this->serialize(s, v);
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::Accepted::serialize(
      elle::serialization::Serializer& serializer,
      elle::Version const& version)
    {
      serializer.serialize("proposal", this->proposal);
      if (version >= elle::Version(0, 1, 0))
      {
        serializer.serialize("value", this->value);
        serializer.serialize("confirmed", this->confirmed);
      }
      else if (serializer.out())
      {
        auto& s = static_cast<elle::serialization::SerializerOut&>(serializer);
        if (this->value.template is<T>())
          s.serialize("value", this->value.template get<T>());
        else
          ELLE_ABORT("Athena cannot serialize quorum changes pre 0.1.0");
      }
      else
      {
        auto& s = static_cast<elle::serialization::SerializerIn&>(serializer);
        this->value.template emplace<T>(s.deserialize<T>("value"));
        this->confirmed = true;
      }
    }

    /*------------.
    | WrongQuorum |
    `------------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::WrongQuorum::WrongQuorum(
      Quorum expected, Quorum effective)
      : elle::Error(
        elle::sprintf("wrong quorum: %f instead of %f", effective, expected))
      , _expected(std::move(expected))
      , _effective(std::move(effective))
    {}

    template <typename T, typename Version, typename CId, typename SId>
    Server<T, Version, CId, SId>::WrongQuorum::WrongQuorum(
      elle::serialization::SerializerIn& input, elle::Version const& v)
      : Super(input)
    {
      this->_serialize(input, v);
    }

    template < typename T, typename Version, typename CId, typename SId>
    void
    Server<T, Version, CId, SId>::WrongQuorum::serialize(
      elle::serialization::Serializer& s, elle::Version const& version)
    {
      Super::serialize(s, version);
      this->_serialize(s, version);
    }

    template <typename T, typename Version, typename CId, typename SId>
    void
    Server<T, Version, CId, SId>::WrongQuorum::_serialize(
      elle::serialization::Serializer& s, elle::Version const& version)
    {
      s.serialize("expected", this->_expected);
      s.serialize("effective", this->_effective);
      if (version < elle::Version(0, 1, 0))
      {
        Version dummy = Version();
        s.serialize("version", dummy);
      }
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    const elle::serialization::Hierarchy<elle::Exception>::Register<
      typename Server<T, Version, ClientId, ServerId>::WrongQuorum>
    Server<T, Version, ClientId, ServerId>::
      _register_wrong_quorum_serialization;

    /*------------.
    | PartialState |
    `------------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::PartialState::PartialState(
      Proposal p)
      : elle::Error(elle::sprintf("partial state: %s", p))
      , _proposal(std::move(p))
    {}

    template <typename T, typename Version, typename CId, typename SId>
    Server<T, Version, CId, SId>::PartialState::PartialState(
      elle::serialization::SerializerIn& input, elle::Version const& v)
      : Super(input)
    {
      this->_serialize(input, v);
    }

    template < typename T, typename Version, typename CId, typename SId>
    void
    Server<T, Version, CId, SId>::PartialState::serialize(
      elle::serialization::Serializer& s, elle::Version const& version)
    {
      Super::serialize(s, version);
      this->_serialize(s, version);
    }

    template <typename T, typename Version, typename CId, typename SId>
    void
    Server<T, Version, CId, SId>::PartialState::_serialize(
      elle::serialization::Serializer& s, elle::Version const& version)
    {
      s.serialize("proposal", this->_proposal);
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    const elle::serialization::Hierarchy<elle::Exception>::Register<
      typename Server<T, Version, ClientId, ServerId>::PartialState>
    Server<T, Version, ClientId, ServerId>::
      _register_partial_state_serialization;

    /*-------------.
    | Construction |
    `-------------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Server(
      ServerId id, Quorum quorum, elle::Version version)
      : _id(std::move(id))
      , _quorum_initial(quorum)
      , _value()
      , _version(version)
      , _state()
    {
      ELLE_ASSERT_CONTAINS(this->_quorum_initial, this->_id);
      this->_register_wrong_quorum_serialization.poke();
      this->_register_partial_state_serialization.poke();
    }

    /*--------.
    | Details |
    `--------*/

    template <typename T, typename Version, typename CId, typename SId>
    struct Server<T, Version, CId, SId>::_Details
    {
      static
      void
      check_quorum(Server<T, Version, CId, SId>& self, Quorum q)
      {
        ELLE_LOG_COMPONENT("athena.paxos.Server");
        if (q != self._quorum_initial)
        {
          ELLE_TRACE("quorum is wrong: %f instead of %f",
                     q, self._quorum_initial);
          throw WrongQuorum(self._quorum_initial, std::move(q));
        }
      }

      /// Check we don't skip any version and the previous version was confirmed
      /// before starting a new one.
      static
      bool
      check_confirmed(Server<T, Version, CId, SId>& self, Proposal const& p)
      {
        if (self.version() < elle::Version(0, 1, 0))
          return true;
        if (!self._state)
          return true;
        auto const& version = self._state->proposal.version;
        if (version >= p.version)
          return true;
        if (version == p.version - 1 &&
            self._state->accepted &&
            self._state->accepted->confirmed)
          return true;
        return false;
      }
    };

    /*----------.
    | Consensus |
    `----------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    boost::optional<typename Server<T, Version, ClientId, ServerId>::Accepted>
    Server<T, Version, ClientId, ServerId>::propose(Quorum q, Proposal p)
    {
      ELLE_LOG_COMPONENT("athena.paxos.Server");
      ELLE_TRACE_SCOPE("%s: get proposal: %s ", *this, p);
      if (this->_state &&
          this->_state->accepted &&
          this->_state->accepted->proposal.version > p.version)
      {
        ELLE_DEBUG(
          "refuse proposal for version %s in favor of version %s",
          p.version, this->_state->accepted->proposal.version);
        return this->_state->accepted;
      }
      if (_Details::check_confirmed(*this, p))
      {
        if (this->_state && p.version > this->_state->proposal.version)
        {
          auto& accepted = this->_state->accepted;
          ELLE_ASSERT(accepted);
          if (accepted->value.template is<T>())
            this->_value.emplace(std::move(accepted->value.template get<T>()));
          else
            this->_quorum_initial =
              std::move(accepted->value.template get<Quorum>());
          this->_state.reset();
        }
        _Details::check_quorum(*this, q);
      }
      else
      {
        // FIXME: if the quorum changed, we need to take note !
        this->_state.reset();
      }
      if (!this->_state)
      {
        ELLE_DEBUG_SCOPE("accept first proposal for version %s", p.version);
        this->_state.emplace(std::move(p));
        return {};
      }
      else
      {
        if (this->_state->proposal < p)
        {
          ELLE_DEBUG("update minimum proposal for version %s", p.version);
          this->_state->proposal = std::move(p);
        }
        return this->_state->accepted;
      }
    }

    template <typename T, typename Version, typename CId, typename SId>
    typename Server<T, Version, CId, SId>::Proposal
    Server<T, Version, CId, SId>::accept(
      Quorum q, Proposal p, elle::Option<T, Quorum> value)
    {
      ELLE_LOG_COMPONENT("athena.paxos.Server");
      ELLE_TRACE_SCOPE("%s: accept for %f: %f", *this, p, value);
      _Details::check_quorum(*this, q);
      if (!this->_state || this->_state->proposal < p)
      {
        ELLE_WARN("%s: someone malicious sent an accept before propose",
                  this);
        throw elle::Error("propose before accepting");
      }
      if (p < this->_state->proposal)
      {
        ELLE_TRACE("discard obsolete accept, current proposal is %s",
                   this->_state->proposal);
        return this->_state->proposal;
      }
      auto& version = *this->_state;
      if (!(p < version.proposal))
      {
        if (!version.accepted)
          version.accepted.emplace(std::move(p), std::move(value), false);
        else
        {
          // FIXME: assert !confirmed || new_value == value ?
          version.accepted->proposal = std::move(p);
          version.accepted->value = std::move(value);
        }
      }
      return version.proposal;
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::confirm(Quorum q, Proposal p)
    {
      ELLE_LOG_COMPONENT("athena.paxos.Server");
      ELLE_TRACE_SCOPE("%s: confirm proposal %s", *this, p);
      _Details::check_quorum(*this, q);
      if (!this->_state ||
          this->_state->proposal < p ||
          !this->_state->accepted)
      {
        ELLE_WARN("%s: someone malicious sent a confirm before propose/accept",
                  this);
        throw elle::Error("propose and accept before confirming");
      }
      if (p < this->_state->proposal)
      {
        ELLE_TRACE("discard obsolete confirm, current proposal is %s",
                   this->_state->proposal);
        return;
      }
      auto& accepted = *this->_state->accepted;
      if (!accepted.confirmed)
        accepted.confirmed = true;
    }

    template <typename T, typename Version, typename CId, typename SId>
    typename Server<T, Version, CId, SId>::Quorum
    Server<T, Version, CId, SId>::current_quorum() const
    {
      if (this->_state &&
          this->_state->accepted &&
          this->_state->accepted->confirmed &&
          this->_state->accepted->value.template is<Quorum>())
        return this->_state->accepted->value.template get<Quorum>();
      else
        return this->_quorum_initial;
    }

    template <typename T, typename Version, typename CId, typename SId>
    boost::optional<typename Server<T, Version, CId, SId>::Accepted>
    Server<T, Version, CId, SId>::current_value() const
    {
      if (!this->_state)
        return {};
      if (this->_state->accepted &&
          this->_state->accepted->confirmed &&
          this->_state->accepted->value.template is<T>())
        return this->_state->accepted;
      else
        if (this->_value)
          return Accepted(this->_state->proposal, *this->_value, true);
        else
          return {};
    }

    template <typename T, typename Version, typename CId, typename SId>
    Version
    Server<T, Version, CId, SId>::current_version() const
    {
      if (this->_state)
      {
        if (this->_state->accepted &&
            this->_state->accepted->confirmed)
          return this->_state->version();
        else
          return this->_state->version() - 1;
      }
      else
        return Version();
    }

    template <typename T, typename Version, typename CId, typename SId>
    boost::optional<typename Server<T, Version, CId, SId>::Accepted>
    Server<T, Version, CId, SId>::get(Quorum q)
    {
      ELLE_LOG_COMPONENT("athena.paxos.Server");
      ELLE_TRACE_SCOPE("%s: get", *this);
      _Details::check_quorum(*this, q);
      return this->current_value();
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::VersionState::VersionState(
      Proposal p, boost::optional<Accepted> a)
      : proposal(std::move(p))
      , accepted(std::move(a))
    {}

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::VersionState::VersionState(
      elle::serialization::SerializerIn& s, elle::Version const& v)
    {
      this->serialize(s, v);
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::VersionState::serialize(
      elle::serialization::Serializer& s, elle::Version const& v)
    {
      s.serialize("proposal", this->proposal);
      s.serialize("accepted", this->accepted);
    }

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Version
    Server<T, Version, ClientId, ServerId>::VersionState::version() const
    {
      return this->proposal.version;
    }

    /*--------------.
    | Serialization |
    `--------------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    Server<T, Version, ClientId, ServerId>::Server(
      elle::serialization::SerializerIn& s, elle::Version const& v)
      : _id()
      , _quorum_initial()
      , _value()
      , _state()
    {
      this->serialize(s, v);
    }

    // FIXME: use splitted serialization
    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::serialize(
      elle::serialization::Serializer& s, elle::Version const& v)
    {
      s.serialize("id", this->_id);
      s.serialize("quorum", this->_quorum_initial);
      if (v >= elle::Version(0, 1, 0))
        s.serialize("value", this->_value);
      typedef boost::multi_index::multi_index_container<
        VersionState,
        boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<
            boost::multi_index::const_mem_fun<
              VersionState, Version, &VersionState::version>>
          >
        > VersionsState;
      if (s.out())
      {
        VersionsState states;
        if (this->_state)
          states.emplace(this->_state.get());
        s.serialize("state", states);
      }
      else
      {
        VersionsState states;
        s.serialize("state", states);
        auto it = states.rbegin();
        if (it != states.rend())
          this->_state.emplace(*it);
      }
    }

    /*----------.
    | Printable |
    `----------*/

    template <
      typename T, typename Version, typename ClientId, typename ServerId>
    void
    Server<T, Version, ClientId, ServerId>::print(std::ostream& output) const
    {
      elle::fprintf(output, "athena::paxos::Server(%f)", this->id());
    }
  }
}

#endif
