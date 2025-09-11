#pragma once

class rhiResource
{
public:
	const u64 get_hash() const { return hash; }
	void set_hash(u64 new_hash) { hash = new_hash; }

protected:
	u64 hash;
};

