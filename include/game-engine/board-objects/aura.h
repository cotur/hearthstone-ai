#pragma once

#include <list>

namespace GameEngine {
	class Board;

	namespace BoardObjects {

		class Minion;
		class MinionManipulator;

		class Aura
		{
			friend std::hash<Aura>;

		public:
			Aura() {}
			virtual ~Aura() {}

			bool operator==(Aura const& rhs) const { return this->EqualsTo(rhs); }
			bool operator!=(Aura const& rhs) const { return !(*this == rhs); }

		public: // hooks
			virtual void AfterAdded(MinionManipulator & owner) {}
			virtual void BeforeRemoved(MinionManipulator & owner) {}

			virtual void HookAfterMinionAdded(MinionManipulator & aura_owner, MinionManipulator & added_minion) {}
			virtual void HookAfterOwnerEnraged(MinionManipulator &enraged_aura_owner) {}
			virtual void HookAfterOwnerUnEnraged(MinionManipulator &unenraged_aura_owner) {}

		protected:
			virtual bool EqualsTo(Aura const& rhs) const = 0; // this is a pure virtual class (i.e., no member to be compared)
			virtual std::size_t GetHash() const = 0; // this is a pure virtual class (i.e., no member to be hashed)
		};

		class Auras
		{
			friend std::hash<Auras>;

		public:
			Auras() {}

			Auras(Auras const& rhs) = delete;
			Auras & operator=(Auras const& rhs) = delete;
			Auras(Auras && rhs) { *this = std::move(rhs); }
			Auras & operator=(Auras && rhs) {
				this->auras = std::move(rhs.auras);
				return *this;
			}

			bool operator==(Auras const& rhs) const
			{
				if (this->auras.size() != rhs.auras.size()) return false;

				auto it_lhs = this->auras.begin();
				auto it_rhs = rhs.auras.begin();

				while (true)
				{
					if (it_lhs == this->auras.end()) break;
					if (it_rhs == rhs.auras.end()) break;

					if (*it_lhs != *it_rhs) return false;
				}
				// both iterators should reach end here, since the size is equal

				return true;
			}

			bool operator!=(Auras const& rhs) const { return !(*this == rhs); }

		public:
			void Add(MinionManipulator & owner, std::unique_ptr<Aura> && aura)
			{
				auto ref_ptr = aura.get();
				this->auras.push_back(std::move(aura));
				ref_ptr->AfterAdded(owner);
			}

			void Clear(MinionManipulator & owner)
			{
				for (auto & aura : this->auras) aura->BeforeRemoved(owner);
				this->auras.clear();
			}

			bool Empty() const { return this->auras.empty(); }

		public: // hooks
			void HookAfterMinionAdded(MinionManipulator & aura_owner, MinionManipulator & added_minion)
			{
				for (auto & aura : this->auras) aura->HookAfterMinionAdded(aura_owner, added_minion);
			}
			void HookAfterOwnerEnraged(MinionManipulator &enraged_aura_owner)
			{
				for (auto & aura : this->auras) aura->HookAfterOwnerEnraged(enraged_aura_owner);
			}
			void HookAfterOwnerUnEnraged(MinionManipulator &unenraged_aura_owner)
			{
				for (auto & aura : this->auras) aura->HookAfterOwnerUnEnraged(unenraged_aura_owner);
			}

		private:
			std::list<std::unique_ptr<Aura>> auras;
		};

	} // namespace BoardObjects
} // namespace GameEngine

namespace std {
	template <> struct hash<GameEngine::BoardObjects::Aura> {
		typedef GameEngine::BoardObjects::Aura argument_type;
		typedef std::size_t result_type;
		result_type operator()(const argument_type &s) const {
			return s.GetHash();
		}
	};
	template <> struct hash<GameEngine::BoardObjects::Auras> {
		typedef GameEngine::BoardObjects::Auras argument_type;
		typedef std::size_t result_type;
		result_type operator()(const argument_type &s) const {
			result_type result = 0;

			for (auto const& aura : s.auras) {
				GameEngine::hash_combine(result, aura);
			}

			return result;
		}
	};
}