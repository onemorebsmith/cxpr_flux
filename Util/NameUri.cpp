#pragma once

//////////////////////////////////////////////////////////////////////////

#include "Kinetic3DPch.h"
#include "K3UI.h"

//////////////////////////////////////////////////////////////////////////

using namespace Kinetic::Gfx;
using namespace cxpr_flux;

//////////////////////////////////////////////////////////////////////////

NameUri NameUri::Append(const NameUri& path, StringViewT Name)
{
	StringT str = path.name.c_str();
	if (str.empty() == false)
	{
		str += ".";
	}
	str += Name;

	return NameUri(str);
}

//////////////////////////////////////////////////////////////////////////

BSTL::JSON::json_ostream& cxpr_flux::operator<<(BSTL::JSON::json_ostream& os, const NameUri& uri)
{
	auto obj = os.Object("NameUri");
	{
		obj.Value("value", uri.Name());
		obj.Value("hash", uri.HashKey());
	}

	return os;
}
