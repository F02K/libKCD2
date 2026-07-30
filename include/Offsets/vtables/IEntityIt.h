#pragma once

// KCD2 1.5.6 CEntityItMap vtable @ WHGame+0x4077008. The seven slots
// were verified against GetEntityIterator (WHGame+0xAE2C34) and the
// concrete slot targets, including MoveFirst at WHGame+0xAE2CC4.

namespace Offsets
{
	struct IEntity;

	struct IEntityIt
	{
		virtual void Dtor(char flags) = 0; // [0]
		virtual void AddRef() = 0; // [1]
		virtual void Release() = 0; // [2]
		virtual bool IsEnd() = 0; // [3]
		virtual IEntity *Next() = 0; // [4]
		virtual IEntity *This() = 0; // [5]
		virtual void MoveFirst() = 0; // [6]
	};
}
