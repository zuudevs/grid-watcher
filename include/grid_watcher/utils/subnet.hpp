#pragma once

#include "clamp.hpp"

namespace gw::util {

	struct Subnet {
		template <std::integral Val>
		[[nodiscard]] constexpr uint32_t operator()(Val prefix_length) noexcept {
			uint32_t len = util::clamp{}(static_cast<uint32_t>(prefix_length), 0u, 32u);
			return (len == 0) ? 0 : (0xFFFFFFFF << (32 - len));
		}
	} ;
} // namespace gw::util
