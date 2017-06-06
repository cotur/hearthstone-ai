#pragma once

#include "MCTS/Board.h"
#include "MCTS/ActionParameterGetter.h"
#include "MCTS/RandomGenerator.h"

namespace mcts
{
	inline int Board::GetActionsCount()
	{
		int idx = 0;
		actions_[idx++] = &Board::PlayCard;
		actions_[idx++] = &Board::Attack;
		actions_[idx++] = &Board::HeroPower;
		actions_[idx++] = &Board::EndTurn;

		return idx;
	}

	inline Result Board::ApplyAction(int action, RandomGenerator & random, ActionParameterGetter & action_parameters) {
		return (this->*(actions_[action]))(random, action_parameters);
	}

	inline Result Board::PlayCard(RandomGenerator & random, ActionParameterGetter & action_parameters)
	{
		FlowControl::FlowContext flow_context(random, action_parameters);
		int hand_count = (int)state_.GetCurrentPlayer().hand_.Size();
		if (hand_count == 0) return Result::kResultInvalid;
		int hand_idx = action_parameters.GetNumber(hand_count);
		return FlowControl::FlowController(state_, flow_context).PlayCard(hand_idx);
	}

	inline Result Board::Attack(RandomGenerator & random, ActionParameterGetter & action_parameters)
	{
		FlowControl::FlowContext flow_context(random, action_parameters);
		// TODO: only find attackable characters
		state::CardRef attacker = UserChooseSideCharacter(state_.GetCurrentPlayerId(), action_parameters);

		// TODO: only find defendable characters
		state::CardRef defender = UserChooseSideCharacter(state_.GetCurrentPlayerId().Opposite(), action_parameters);
		return FlowControl::FlowController(state_, flow_context).Attack(attacker, defender);
	}

	inline Result Board::HeroPower(RandomGenerator & random, ActionParameterGetter & action_parameters)
	{
		FlowControl::FlowContext flow_context(random, action_parameters);
		return FlowControl::FlowController(state_, flow_context).HeroPower();
	}

	inline Result Board::EndTurn(RandomGenerator & random, ActionParameterGetter & action_parameters)
	{
		FlowControl::FlowContext flow_context(random, action_parameters);
		return FlowControl::FlowController(state_, flow_context).EndTurn();
	}

	inline state::CardRef Board::UserChooseSideCharacter(state::PlayerIdentifier player_id, ActionParameterGetter & action_parameters)
	{
		int attacker_count = (int)state_.GetBoard().Get(player_id).minions_.Size() + 1;
		int attacker_idx = action_parameters.GetNumber(attacker_count);

		if (attacker_idx == 0) return state_.GetBoard().Get(player_id).GetHeroRef();

		attacker_idx--;
		return state_.GetBoard().Get(player_id).minions_.Get(attacker_idx);
	}
}