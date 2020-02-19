#pragma once

//////////////////////////////////////////////////////////////////////////

#include "Kinetic3DPch.h"

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	struct NameUri
	{
		constexpr NameUri() noexcept: name{}, hash{}{}
		constexpr NameUri(const NameUri& other) : name{ other.name }, hash{ other.hash }{}
		constexpr NameUri::NameUri(StringViewT inUri) : name(inUri), hash{}
		{
			hash = BSTL::StringUtils::HashStringInvariant(inUri);
		}
		constexpr NameUri(const char* inUri) : NameUri(StringViewT(inUri)) {}

		constexpr bool operator==(const NameUri& other) { return other.hash == hash; }

		BSTL::hash_t HashKey() const { return hash; }
		StringViewT Name() const	 { return name; }
		bool IsEmpty() const { return name.size() == 0; }

		/// Appends Name to the end of path, with the approprate delim
		static NameUri Append(const NameUri& path, StringViewT Name);
	private:
		BSTL::hash_t hash;
		BSTL::String64 name; // change to heap-string when it's cxpr friendly
		//StringT name;
	};

	BSTL::JSON::json_ostream& operator<<(BSTL::JSON::json_ostream& os, const cxpr_flux::NameUri& uri);
}