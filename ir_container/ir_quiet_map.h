/*
	Part of the Ironic Project. Redistributed under MIT License, which means:
		- Do whatever you want
		- Please keep this notice and include the license file to your project
		- I provide no warranty
	To get help with installation, visit README
	Created by github.com/Meta-chan, k.sovailo@gmail.com
	Reinventing bicycles since 2020
*/

#ifndef IR_QUIET_MAP
#define IR_QUIET_MAP

#include <ir_syschar.h>
#include <ir_container/ir_quiet_vector.h>

namespace ir
{
///@addtogroup ir_container
///@{
	
	#ifndef DOXYGEN
		template <class K, class V> struct MapElement
		{
			K key;
			V value;
		};
	#endif
	

	///Ironic library's quiet map @n
	///It is sorted ir::QuietVector with binary search @n
	///`QuietMap` is designed to be fast and reliable, which means:
	/// - `QuietMap` throws no exceptions.
	/// - `QuietMap` indicates leak of memory through `false` as return value
	/// - `QuietMap` checks if elements exist with `assert` and may cause critical failure.
	/// - `QuietMap` does not check if map is unique
	///@tparam K Key type
	///@tparam V Value type
	///@tparam C Comparator - a class with `static int compare(const K &k1, const K &k2) noexcept` member
	template <class K, class V, class C> class QuietMap : private QuietVector<MapElement<K, V>>
	{
	private:
		typedef QuietVector<MapElement<K, V>> super;
		mutable size_t _last = 0;
		bool _find(const K &key, size_t *position)		const noexcept;

	public:
		//Constuctors:
		///Creates a map
		QuietMap()										noexcept;
		///Copies map
		///@param reg Map to be copied
		QuietMap(const QuietMap &map)					noexcept;
		///Assigns one map to another
		///@param map Map to be assigned to
		const QuietMap &assign(const QuietMap &map)		noexcept;
		///Assigns one map to another
		///@param map Map to be assigned to
		const QuietMap &operator=(const QuietMap &map)	noexcept;

		//Non-constant element access:
		///Returns reference to element with specified identifier
		///@param key Element identifier
		inline V &operator[](const K &key)				noexcept;
		///Returns reference to element with specified identifier
		///@param key Element identifier
		inline V &at(const K &key)						noexcept;
		///Assigns value to element with specified identifier
		///@param key Element identifier
		///@param elem Value to be assigned
		inline bool set(const K &key, const V &elem)	noexcept;
		///Deletes an element with given identifier
		///@param key Element identifier
		inline void erase(const K &key)					noexcept;
		///Returns reference to element on specified position, used for iterations over map
		///@param index Position of required element in map
		inline V &direct_value(size_t index)			noexcept;
		
		//Constant element access:
		///Returns constant reference to element with specified identifier
		///@param key Element identifier
		inline const V &operator[](const K &key)		const noexcept;
		///Returns constant reference to element with specified identifier
		///@param key Element identifier
		inline const V &at(const K &key)				const noexcept;
		///Checks if map contains an element with given identifier
		///@param key Element identifier
		inline bool has(const K &key)					const noexcept;
		///Returns constant reference to element on specified position, used for iterations over map
		///@param index Position of required element in map
		inline const V &direct_value(size_t index)		const noexcept;
		///Returns identifier of element on specified position, used for iterations over map
		///@param index Position of required element in map
		inline const K &direct_key(size_t index)		const noexcept;
		
		//Maintenance:
		using super::empty;
		using super::size;
		using super::capacity;
		using super::reserve;
		using super::clear;
		using super::detach;
	};
	
///@}
}

#if (defined(IR_IMPLEMENT) || defined(IR_QUIET_MAP_IMPLEMENT)) && !defined(IR_QUIET_MAP_NOT_IMPLEMENT)
	#include "../implementation/ir_container/ir_quiet_map_implementation.h"
#endif

#endif //#ifndef IR_QUIET_MAP