#pragma once

#include <utility>
#include <unordered_map>
#include "EventManager/HandlersContainer.h"

namespace EventManager
{
	template <typename CategoryType, typename HandlerType>
	class CategorizedHandlersContainer
	{
	public:
		// Cloneable by copy semantics
		//    Since the STL container and HandlersContainer are with this property
		static const bool CloneableByCopySemantics = true;

		void PushBack(const CategoryType& category, HandlerType&& handler)
		{
			categories_[category].PushBack(std::forward<HandlerType>(handler));
		}

		template <typename... Args>
		void TriggerAll(const CategoryType& category, Args&&... args)
		{
			categories_[category].TriggerAll(std::forward<Args>(args)...);
		}

	private:
		std::unordered_map<CategoryType, HandlersContainer<HandlerType>> categories_;
	};
}