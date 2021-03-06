#pragma once

#include <random>
#include "engine/view/Board.h"
#include "MCTS/Types.h"
#include "MCTS/policy/RandomByRand.h"
#include "neural_net/NeuralNetwork.h"

namespace mcts
{
	namespace policy
	{
		namespace simulation
		{
			class ChoiceGetter
			{
			public:
				ChoiceGetter(int choices) : choices_(choices) {}

				size_t Size() const { return (size_t)choices_; }

				int Get(size_t idx) const { return (int)idx; }

				template <typename Functor>
				void ForEachChoice(Functor&& functor) const {
					for (int i = 0; i < choices_; ++i) {
						if (!functor(i)) return;
					}
				}

			private:
				int choices_;
			};

			class RandomPlayouts
			{
			public:
				static constexpr bool kEnableCutoff = false;

				RandomPlayouts(std::mt19937 & rand, Config const& config) :
					rand_(rand)
				{
				}

				engine::Result GetCutoffResult(engine::view::Board const& board) {
					assert(false);
					return engine::kResultNotDetermined;
				}

				void StartAction(engine::view::Board const& board, engine::ValidActionAnalyzer const& action_analyzer) {
					(void)board;
					(void)action_analyzer;
				}

				int GetChoice(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer,
					engine::ActionType action_type,
					ChoiceGetter const& choice_getter)
				{
					size_t count = choice_getter.Size();
					assert(count > 0);
					size_t rand_idx = (size_t)(rand_() % count);
					int result = choice_getter.Get(rand_idx);
					assert([&]() {
						int result2 = -1;
						size_t idx = 0;
						choice_getter.ForEachChoice([&](int choice) {
							if (idx == rand_idx) {
								result2 = choice;
								return false;
							}
							++idx;
							return true;
						});
						return result == result2;
					}());
					return result;
				}

			private:
				std::mt19937 & rand_;
			};

			class RandomCutoff
			{
			public:
				static constexpr bool kEnableCutoff = true;
				static constexpr bool kRandomlyPutMinions = true;

				RandomCutoff(std::mt19937 & rand, Config const& config) :
					rand_(rand)
				{
				}

				engine::Result GetCutoffResult(engine::view::Board const& board) {
					bool win = (rand_() % 2 == 0);
					if (win) return engine::kResultFirstPlayerWin;
					else return engine::kResultSecondPlayerWin;
				}

				void StartAction(engine::view::Board const& board, engine::ValidActionAnalyzer const& action_analyzer) {
					(void)board;
					(void)action_analyzer;
				}

				int GetChoice(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer,
					engine::ActionType action_type,
					ChoiceGetter const& choice_getter)
				{
					size_t count = choice_getter.Size();
					assert(count > 0);
					size_t rand_idx = (size_t)(rand_() % count);
					int result = choice_getter.Get(rand_idx);
					assert([&]() {
						int result2 = -1;
						size_t idx = 0;
						choice_getter.ForEachChoice([&](int choice) {
							if (idx == rand_idx) {
								result2 = choice;
								return false;
							}
							++idx;
							return true;
						});
						return result == result2;
					}());
					return result;
				}

			private:
				std::mt19937 & rand_;
			};

			class NeuralNetworkStateValueFunction
			{
			public:
				NeuralNetworkStateValueFunction(Config const& config, std::mt19937 & random)
					: net_(), current_player_viewer_(), random_(random)
				{
					net_.Load(config.GetNeuralNetPath(), config.IsNeuralNetRandom());
				}

				StateValue GetStateValue(engine::view::Board const& board) {
					return GetStateValue(board.RevealHiddenInformationForSimulation());
				}

				StateValue GetStateValue(state::State const& state) {
					current_player_viewer_.Reset(state);

					float score = (float)net_.Predict(&current_player_viewer_, random_);

					if (score > 1.0f) score = 1.0f;
					if (score < -1.0f) score = -1.0f;

					StateValue ret;
					ret.SetValue(score, state.GetCurrentPlayerId().GetSide());
					return ret;
				}

			private:
				// TODO: move to nerual_net namespace
				class StateDataBridge : public neural_net::IInputGetter
				{
				public:
					StateDataBridge() : state_(nullptr), attackable_indices_(),
						playable_cards_(), hero_power_playable_()
					{}

					StateDataBridge(StateDataBridge const&) = delete;
					StateDataBridge & operator=(StateDataBridge const&) = delete;

					void Reset(state::State const& state) {
						state_ = &state;

						engine::FlowControl::ValidActionGetter valid_action(*state_);

						attackable_indices_.clear();
						valid_action.ForEachAttacker([this](int encoded_idx) {
							attackable_indices_.push_back(encoded_idx);
							return true;
						});

						playable_cards_.clear();
						valid_action.ForEachPlayableCard([&](size_t idx) {
							playable_cards_.push_back((int)idx);
							return true;
						});

						hero_power_playable_ = valid_action.CanUseHeroPower();
					}

					double GetField(
						neural_net::FieldSide field_side,
						neural_net::FieldType field_type,
						int arg1 = 0) const override final
					{
						if (field_side == neural_net::FieldSide::kCurrent) {
							return GetSideField(field_type, arg1, state_->GetCurrentPlayer());
						}
						else if (field_side == neural_net::FieldSide::kOpponent) {
							return GetSideField(field_type, arg1, state_->GetOppositePlayer());
						}
						throw std::runtime_error("invalid side");
					}

				private:
					double GetSideField(neural_net::FieldType field_type, int arg1, state::board::Player const& player) const {
						switch (field_type) {
						case neural_net::FieldType::kResourceCurrent:
						case neural_net::FieldType::kResourceTotal:
						case neural_net::FieldType::kResourceOverload:
						case neural_net::FieldType::kResourceOverloadNext:
							return GetResourceField(field_type, arg1, player.GetResource());

						case neural_net::FieldType::kHeroHP:
						case neural_net::FieldType::kHeroArmor:
							return GetHeroField(field_type, arg1, state_->GetCard(player.GetHeroRef()));

						case neural_net::FieldType::kMinionCount:
						case neural_net::FieldType::kMinionHP:
						case neural_net::FieldType::kMinionMaxHP:
						case neural_net::FieldType::kMinionAttack:
						case neural_net::FieldType::kMinionAttackable:
						case neural_net::FieldType::kMinionTaunt:
						case neural_net::FieldType::kMinionShield:
						case neural_net::FieldType::kMinionStealth:
							return GetMinionsField(field_type, arg1, player.minions_);

						case neural_net::FieldType::kHandCount:
						case neural_net::FieldType::kHandPlayable:
						case neural_net::FieldType::kHandCost:
							return GetHandField(field_type, arg1, player.hand_);

						case neural_net::FieldType::kHeroPowerPlayable:
							return GetHeroPowerField(field_type, arg1);

						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetResourceField(neural_net::FieldType field_type, int arg1, state::board::PlayerResource const& resource) const {
						switch (field_type) {
						case neural_net::FieldType::kResourceCurrent:
							return resource.GetCurrent();
						case neural_net::FieldType::kResourceTotal:
							return resource.GetTotal();
						case neural_net::FieldType::kResourceOverload:
							return resource.GetCurrentOverloaded();
						case neural_net::FieldType::kResourceOverloadNext:
							return resource.GetNextOverload();
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetHeroField(neural_net::FieldType field_type, int arg1, state::Cards::Card const& hero) const {
						switch (field_type) {
						case neural_net::FieldType::kHeroHP:
							return hero.GetHP();
						case neural_net::FieldType::kHeroArmor:
							return hero.GetArmor();
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetMinionsField(neural_net::FieldType field_type, int minion_idx, state::board::Minions const& minions) const {
						switch (field_type) {
						case neural_net::FieldType::kMinionCount:
							return (double)minions.Size();
						case neural_net::FieldType::kMinionHP:
						case neural_net::FieldType::kMinionMaxHP:
						case neural_net::FieldType::kMinionAttack:
						case neural_net::FieldType::kMinionAttackable:
						case neural_net::FieldType::kMinionTaunt:
						case neural_net::FieldType::kMinionShield:
						case neural_net::FieldType::kMinionStealth:
							return GetMinionField(field_type, minion_idx, state_->GetCard(minions.Get(minion_idx)));
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetMinionField(neural_net::FieldType field_type, int minion_idx, state::Cards::Card const& minion) const {
						switch (field_type) {
						case neural_net::FieldType::kMinionHP:
							return minion.GetHP();
						case neural_net::FieldType::kMinionMaxHP:
							return minion.GetMaxHP();
						case neural_net::FieldType::kMinionAttack:
							return minion.GetAttack();
						case neural_net::FieldType::kMinionAttackable:
							for (auto target_index : attackable_indices_) {
								if (engine::FlowControl::ActionTargetIndex::ParseMinionIndex(target_index) == minion_idx) {
									return true;
								}
							}
							return false;
						case neural_net::FieldType::kMinionTaunt:
							return minion.HasTaunt();
						case neural_net::FieldType::kMinionShield:
							return minion.HasShield();
						case neural_net::FieldType::kMinionStealth:
							return minion.HasStealth();
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetHandField(neural_net::FieldType field_type, int hand_idx, state::board::Hand const& hand) const {
						switch (field_type) {
						case neural_net::FieldType::kHandCount:
							return (double)hand.Size();
						case neural_net::FieldType::kHandPlayable:
						case neural_net::FieldType::kHandCost:
							return GetHandCardField(field_type, hand_idx, state_->GetCard(hand.Get(hand_idx)));
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetHandCardField(neural_net::FieldType field_type, int hand_idx, state::Cards::Card const& card) const {
						switch (field_type) {
						case neural_net::FieldType::kHandPlayable:
							return (std::find(playable_cards_.begin(), playable_cards_.end(), hand_idx) != playable_cards_.end());
						case neural_net::FieldType::kHandCost:
							return card.GetCost();
						default:
							throw std::runtime_error("unknown field type");
						}
					}

					double GetHeroPowerField(neural_net::FieldType field_type, int arg1) const {
						switch (field_type) {
						case neural_net::FieldType::kHeroPowerPlayable:
							return hero_power_playable_;
						default:
							throw std::runtime_error("unknown field type");
						}
					}

				private:
					state::State const* state_;
					std::vector<int> attackable_indices_;
					std::vector<int> playable_cards_;
					bool hero_power_playable_;
				};

			private:
				neural_net::NeuralNetwork net_;
				StateDataBridge current_player_viewer_;
				std::mt19937 & random_;
			};

			class RandomPlayoutWithHeuristicEarlyCutoffPolicy
			{
			public:
				// Simulation cutoff:
				// For each simulation choice, a small probability is defined so the simulation ends here,
				//    and the game result is defined by a heuristic state-value function.
				// Assume the cutoff probability is p,
				//    The expected number of simulation runs is 1/p.
				//    So, if the expected number of runs is N, the probability p = 1.0 / N
				static constexpr bool kEnableCutoff = true;
				static constexpr double kCutoffExpectedRuns = 10;
				static constexpr double kCutoffProbability = 1.0 / kCutoffExpectedRuns;

				bool GetCutoffResult(engine::view::Board const& board, StateValue & state_value) {
					std::uniform_real_distribution<double> rand_gen(0.0, 1.0);
					double v = rand_gen(rand_);
					if (v >= kCutoffProbability) return false;

					state_value = state_value_func_.GetStateValue(board);
					return true;
				}

			public:
				RandomPlayoutWithHeuristicEarlyCutoffPolicy(state::PlayerSide side, std::mt19937 & rand, Config & config) :
					rand_(rand),
					state_value_func_(config, rand_)
				{
				}

				void StartAction(engine::view::Board const& board, engine::ValidActionAnalyzer const& action_analyzer) {
					(void)board;
					(void)action_analyzer;
				}

				int GetChoice(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer,
					engine::ActionType action_type,
					ChoiceGetter const& choice_getter)
				{
					size_t count = choice_getter.Size();
					assert(count > 0);
					size_t rand_idx = (size_t)(rand_() % count);
					return choice_getter.Get(rand_idx);
				}

			private:
				std::mt19937 & rand_;
				NeuralNetworkStateValueFunction state_value_func_;
			};

			class HeuristicPlayoutWithHeuristicEarlyCutoffPolicy
			{
			public:
				// Simulation cutoff:
				// For each simulation choice, a small probability is defined so the simulation ends here,
				//    and the game result is defined by a heuristic state-value function.
				// Assume the cutoff probability is p,
				//    The expected number of simulation runs is 1/p.
				//    So, if the expected number of runs is N, the probability p = 1.0 / N
				static constexpr bool kEnableCutoff = true;
				static constexpr double kCutoffExpectedRuns = 0.5;
				static constexpr double kCutoffProbability = 1.0 / kCutoffExpectedRuns;
				static constexpr bool kRandomlyPutMinions = true;

				bool GetCutoffResult(engine::view::Board const& board, StateValue & state_value) {
					std::uniform_real_distribution<double> rand_gen(0.0, 1.0);
					double v = rand_gen(rand_);
					if (v >= kCutoffProbability) return false;

					state_value = state_value_func_.GetStateValue(board);
					return true;
				}

			public:
				HeuristicPlayoutWithHeuristicEarlyCutoffPolicy(std::mt19937 & rand, Config const& config) :
					rand_(rand),
					decision_(), decision_idx_(0),
					state_value_func_(config, rand_)
				{
				}

				void StartAction(engine::view::Board const& board, engine::ValidActionAnalyzer const& action_analyzer) {
					StartNewAction(board, action_analyzer);
				}

				int GetChoice(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer,
					engine::ActionType action_type,
					ChoiceGetter const& choice_getter)
				{
					if constexpr (kRandomlyPutMinions) {
						return GetChoiceRandomly(action_analyzer, choice_getter);
					}
					else {
						return GetChoiceForMainAction(action_analyzer, choice_getter);
					}
				}

			private:
				void StartNewAction(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer)
				{
					decision_idx_ = 0;
					DFSBestStateValue(board, action_analyzer);
				}

				void DFSBestStateValue(
					engine::view::Board const& board,
					engine::ValidActionAnalyzer const& action_analyzer)
				{
					struct DFSItem {
						size_t choice_;
						size_t total_;

						DFSItem(size_t choice, size_t total) : choice_(choice), total_(total) {}
					};

					class UserChoicePolicy : public engine::IActionParameterGetter {
					public:
						UserChoicePolicy(std::vector<DFSItem> & dfs,
							std::vector<DFSItem>::iterator & dfs_it,
							int seed) :
							dfs_(dfs), dfs_it_(dfs_it), rand_(seed), main_op_idx_(-1)
						{}

						// TODO: can we remove this? need special care on main op?
						void SetMainOpIndex(int main_op_idx) { main_op_idx_ = main_op_idx; }

						int GetNumber(engine::ActionType::Types action_type, engine::ActionChoices & action_choices) final {
							if (action_type == engine::ActionType::kMainAction) {
								return main_op_idx_;
							}

							int total = action_choices.Size();

							assert(total >= 1);
							if (total == 1) return 0;

							if (action_type == engine::ActionType::kRandom) {
								return rand_.GetRandom(total);
							}

							assert(action_type != engine::ActionType::kRandom);

							if constexpr (kRandomlyPutMinions) {
								if (action_type == engine::ActionType::kChooseMinionPutLocation) {
									assert(total >= 1);
									int idx = rand_.GetRandom(total);
									return action_choices.Get(idx);
								}
							}

							if (dfs_it_ == dfs_.end()) {
								assert(total >= 1);
								dfs_.emplace_back(0, (size_t)total);
								dfs_it_ = dfs_.end();
								return action_choices.Get(0);
							}

							assert(dfs_it_->total_ == (size_t)total);
							size_t idx = dfs_it_->choice_;
							assert((int)idx < total);
							++dfs_it_;
							return action_choices.Get(idx);
						}

					private:
						std::vector<DFSItem> & dfs_;
						std::vector<DFSItem>::iterator & dfs_it_;
						FastRandom rand_;
						int main_op_idx_;
					};

					std::vector<DFSItem> dfs;
					std::vector<DFSItem>::iterator dfs_it = dfs.begin();

					auto step_next_dfs = [&]() {
						while (!dfs.empty()) {
							if ((dfs.back().choice_ + 1) < dfs.back().total_) {
								break;
							}
							dfs.pop_back();
						}

						if (dfs.empty()) return false; // all done

						++dfs.back().choice_;
						return true;
					};

					// Need to fix a random sequence for a particular run
					// Since, some callbacks might depend on a random
					// For example, choose one card from randomly-chosen three cards
					UserChoicePolicy cb_user_choice(dfs, dfs_it, rand_());
					cb_user_choice.Initialize(board.GetCurrentPlayerStateRefView().GetValidActionGetter());

					auto side = board.GetCurrentPlayer();

					double best_value = -std::numeric_limits<double>::infinity();
					action_analyzer.ForEachMainOp([&](size_t main_op_idx, engine::MainOpType main_op) {
						cb_user_choice.SetMainOpIndex((int)main_op_idx);

						while (true) {
							engine::Game copied_game;
							engine::view::Board copy_board(copied_game, board.GetViewSide());
							copy_board.RefCopyFrom(board);

							dfs_it = dfs.begin();
							auto result = copy_board.ApplyAction(cb_user_choice);

							if (result != engine::kResultInvalid) {
								StateValue state_value;
								if (result == engine::kResultNotDetermined) {
									state_value = state_value_func_.GetStateValue(copy_board);
								}
								else {
									state_value.SetValue(result);
								}

								float value = state_value.GetValue(side.GetSide());

								if (decision_.empty() || value > best_value) {
									best_value = value;

									decision_.clear();
									decision_.push_back((int)main_op_idx);
									for (auto const& item : dfs) {
										decision_.push_back((int)item.choice_);
									}
								}
							}

							if (!step_next_dfs()) break; // all done
						}
						return true;
					});
				}

				int GetChoiceForMainAction(
					engine::ValidActionAnalyzer const& action_analyzer,
					ChoiceGetter const& choice_getter)
				{
					if (decision_idx_ < decision_.size()) {
						return decision_[decision_idx_++];
					}

					// TODO: We fix the random when running dfs search,
					// and in concept, a random might change the best decision.
					// For example, a choose-one with randomly-chosen three cards,
					// we definitely need to re-run the DFS after the random cards are shown.
					// Here, use a pure random choice in these cases
					return GetChoiceRandomly(action_analyzer, choice_getter);
				}

				int GetChoiceRandomly(
					engine::ValidActionAnalyzer const& action_analyzer,
					ChoiceGetter const& choice_getter)
				{
					size_t count = choice_getter.Size();
					assert(count > 0);
					size_t idx = 0;
					size_t rand_idx = (size_t)(rand_() % count);
					int result = -1;
					choice_getter.ForEachChoice([&](int choice) {
						if (idx == rand_idx) {
							result = choice;
							return false;
						}
						++idx;
						return true;
					});
					assert([&]() {
						int result2 = choice_getter.Get(rand_idx);
						return result == result2;
					}());
					return result;
				}

			private:
				std::mt19937 & rand_;

				std::vector<int> decision_;
				size_t decision_idx_;
				NeuralNetworkStateValueFunction state_value_func_;
			};
		}
	}
}
