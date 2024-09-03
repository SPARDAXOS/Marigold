#include "Allocator.h"
#include <algorithm>
#include <memory>
#include <math.h>
#include <concepts>
#include <functional>
#include <cassert>

#ifndef CONTAINER_H
#define CONTAINER_H


namespace Marigold {

	inline constexpr auto INVALID_INDEX = -1;
	inline constexpr std::size_t REALLOCATION_FACTOR = 2;

	template <typename T>
	concept IsPointer = std::is_pointer_v<T>;

	template<
		class T,
		class Alloc = CustomAllocator<T>
	>
	class Container final {
	public:
		using Type = T;
		using Allocator = Alloc;
		using SizeType = std::size_t;
		using Pointer = Type*;
		using ConstantPointer = const T*;
		using InitializerList = std::initializer_list<Type>;
		using ReverseIterator = std::reverse_iterator<Pointer>;
		using ReverseConstantIterator = std::reverse_iterator<ConstantPointer>;
		using Reference = T&;
		using ConstantReference = const T&;
		using Predicate = std::function<bool(const T&)>;
		using DifferenceType = std::ptrdiff_t;
		using AllocatorTraits = std::allocator_traits<CustomAllocator<Type>>;



		static_assert(std::is_object_v<T>, "The C++ Standard forbids containers of non-object types "
			"because of [container.requirements].");

	public: //Ctors
		constexpr Container() noexcept (noexcept(Allocator())) {};
		constexpr explicit Container(const Allocator& allocator) noexcept
			: m_Allocator(allocator)
		{
		}
		constexpr Container(const SizeType count, ConstantReference value, const Allocator& allocator = Allocator())
			: m_Allocator(allocator)
		{
			allocate_and_copy_construct(count, count, value);
		} 
		constexpr explicit Container(SizeType count, const Allocator& allocator = Allocator())
			: m_Allocator(allocator)
		{
			allocate_and_copy_construct(count, count);
		}
		template<IsPointer InputIterator>
		constexpr Container(InputIterator first, InputIterator last, const Allocator& allocator = Allocator()) {
			m_Allocator = allocator;
			assign(first, last);
		}
		constexpr Container(std::initializer_list<Type> list, const Allocator& allocator = Allocator()) 
			: m_Allocator(allocator)
		{
			reserve(list.size());
			construct_from_list(list);
		}

		//Copy Semantics
		constexpr Container(const Container& other)
			: m_Allocator(AllocatorTraits::select_on_container_copy_construction(other.m_Allocator))
		{
			if (!other.m_Iterator)
				return;

			reserve(other.m_Size);
			uninitialized_copy_construct(other);
		}
		constexpr Container(const Container& other, const Allocator& allocator)
			: m_Allocator(allocator)
		{
			if (!other.m_Iterator)
				return;

			reserve(other.m_Size);
			uninitialized_copy_construct(other);
		}
		Container& operator=(const Container& other) noexcept {
			if (this == &other)
				return *this;

			if (other.m_Iterator) {
				if constexpr (AllocatorTraits::propagate_on_container_copy_assignment::value) {
					auto OldAllocator = this->m_Allocator;
					this->m_Allocator = other.m_Allocator;
					if (this->m_Allocator != OldAllocator) {
						if (other.m_Size <= m_Capacity)
							swap_allocator_memory(OldAllocator, m_Allocator, m_Capacity);
						else
							swap_allocator_memory(OldAllocator, m_Allocator, other.capacity());

						copy_assign(other);
						return *this;
					}
				}

				if (other.size() > this->m_Capacity)
					reallocate(other.capacity());
				copy_assign(other);
				return *this;
			}

			clear();
			return *this;
		}

		//Move Semantics
		constexpr Container(Container&& other) noexcept //Fixed: IT WORKS! Not sure. Im not moving the elements. check vector behavior. Might be fine to leave it
			: m_Allocator(std::move(other.m_Allocator)), m_Iterator(other.m_Iterator), m_Size(other.m_Size), m_Capacity(other.m_Capacity)
		{
			other.wipe();
		}
		constexpr Container(Container&& other, const Allocator& allocator)  //Fixed: Requires a test
			: m_Size(other.m_Size), m_Capacity(other.m_Capacity), m_Allocator(std::move(allocator))
		{
			//flip them?
			if (allocator != other.get_allocator()) { 
				uninitialized_allocate_and_move(std::move(other));
			}
			else {
				m_Iterator = other.m_Iterator;//? wot hérew warning
				other.wipe();
			}
		}
		Container& operator=(Container&& other) noexcept {
			if (this == &other)
				return *this;

			if constexpr (AllocatorTraits::propagate_on_container_move_assignment::value) {
				destruct_and_deallocate();
				this->m_Allocator = other.get_allocator();
				this->m_Iterator = other.m_Iterator;
				this->m_Size = other.m_Size;
				this->m_Capacity = other.m_Capacity;
				other.wipe();
			}
			else if (!AllocatorTraits::propagate_on_container_move_assignment::value && this->m_Allocator == other.get_allocator()) {
				destruct_and_deallocate();
				this->m_Iterator = other.m_Iterator;
				this->m_Size = other.m_Size;
				this->m_Capacity = other.m_Capacity;
				other.wipe();
			}
			else {
				clear();
				if (other.size() > this->m_Capacity)
					reallocate(other.capacity());

				for (SizeType i = 0; i < other.size(); i++) {
					AllocatorTraits::construct(m_Allocator, m_Iterator + i, std::move(*(other.m_Iterator + i)));
					m_Size++;
				}

				other.destruct_and_deallocate();
				other.wipe();
			}

			return *this;
		}

		//Dtor
		~Container() {
			destruct_and_deallocate();
		}

		//NOT TESTED
		constexpr Container& operator=(InitializerList ilist) { //Requires a test.
			clear(); //Should really make a destory function. Cause thats what i want out of this most of the time. might be fine tho. Its just that size gets changed too. Otherwise 
			//its just destroy(first, last) really.
			if (ilist.size() > capacity())
				reallocate(ilist.size()); //sus af

			m_Size = ilist.size();
			for (SizeType i = 0; i < ilist.size(); i++)
				AllocatorTraits::construct(m_Allocator, m_Iterator + i, ilist[i]);

			return *this;
		}

	public: //Access - ALL GOOD!
		constexpr inline Reference at(SizeType index) {
			if (index >= m_Size)
				throw std::out_of_range("Access Violation - " + index);

			return m_Iterator[index];
		}
		constexpr inline ConstantReference at(SizeType index) const {
			if (index >= m_Size)
				throw std::out_of_range("Access Violation - " + index);

			return m_Iterator[index];
		}

		constexpr inline Pointer data() noexcept { 
			if (size() <= 0)
				return nullptr;
			return m_Iterator; 
		}
		constexpr inline ConstantPointer data() const noexcept { 
			if (size() <= 0)
				return nullptr;
			return m_Iterator; 
		}

		constexpr inline Reference front() noexcept { return m_Iterator[0]; }
		constexpr inline ConstantReference front() const noexcept { return m_Iterator[0]; }

		constexpr inline Reference back() noexcept { return m_Iterator[m_Size - 1]; }
		constexpr inline ConstantReference back() const noexcept { return m_Iterator[m_Size - 1]; }

		constexpr inline Reference operator[](SizeType index) noexcept { 
			assert(index < size() && "Index out of range");
			return m_Iterator[index]; 
		} 
		constexpr inline ConstantReference operator[](SizeType index) const noexcept { 
			assert(index < size() && "Index out of range");
			return m_Iterator[index]; 
		} 

	public: //Insertion

		// IT WORKS!
		constexpr inline void push_back(ConstantReference value) {
			emplace_back(value);
		}

		// IT WORKS!
		constexpr inline void push_back(Type&& value) {
			emplace_back(std::move(value));
		}

		// IT WORKS!
		template<class... args>
		constexpr inline Pointer emplace(ConstantPointer address, args&&... arguments) { 
			assert(address <= end() && "Vector's argument out of range.");

			SizeType IndexPosition = 0;
			if (address == end())
				IndexPosition = m_Size;
			else
				IndexPosition = std::distance<ConstantPointer>(begin(), address);

			if (m_Capacity == 0)
				reserve(1);
			else if (m_Size == m_Capacity)
				reserve(m_Capacity * REALLOCATION_FACTOR);

			if (m_Iterator + IndexPosition == end()) {
				try {
					AllocatorTraits::construct(m_Allocator, m_Iterator + size(), std::forward<args>(arguments)...);
				}
				catch (...) {
					destruct(m_Iterator + size());
					throw;
				}
			}
			else
				construct_and_shift(IndexPosition, std::forward<args>(arguments)...);

			m_Size++;
			return m_Iterator + IndexPosition;
		}

		// IT WORKS!
		template<class... args>
		constexpr inline Reference emplace_back(args&&... arguments) {
			emplace(end(), std::forward<args>(arguments)...);
			return *(m_Iterator + (m_Size - 1));
		}


		constexpr inline Pointer insert(ConstantPointer position, ConstantReference value) {
			return emplace(position, value);
		}
		constexpr inline Pointer insert(ConstantPointer position, Type&& value) {
			return emplace(position, std::move(value));
		}
		constexpr inline Pointer insert(ConstantPointer position, SizeType count, ConstantReference value) {

		}
		template<class InputIterator>
		constexpr Pointer insert(ConstantPointer position, InputIterator first, InputIterator last) {

		}
		constexpr Pointer insert(ConstantPointer position, InitializerList ilist) {

		}


		//All of these are unfinished!
		constexpr void assign(SizeType count, const Reference value) { //Fixed: Requires a test!
			if (m_Size > 0)
				clear();

			if (count > m_Capacity)
				reserve(count);

			for (SizeType i = 0; i < count; i++)
				emplace(m_Iterator + i, value);
		}
		template<typename InputIt>
		constexpr void assign(InputIt first, InputIt last) { //Fixed: Requires a test!
			SizeType size = std::distance(first, last);
			if (size > m_Capacity)
				reserve(size);

			if (m_Size != 0) {
				if (size > m_Size)
					destruct(begin(), end());
				else
					destruct(m_Iterator, m_Iterator + size);
			}

			for (SizeType index = 0; index < size; index++) {
				AllocatorTraits::construct(m_Allocator, m_Iterator + index, *(first + index));
				m_Size++;
			}
		}

		//IT WORKS! but recheck it
		constexpr void assign(InitializerList list) { 
			if (m_Size > 0)
				clear();

			if (list.size() > m_Capacity)
				reserve(list.size());

			construct_from_list(list);
		}

	public: //Removal
		//IT WORKS!
		constexpr inline void clear() noexcept {
			if (m_Size == 0)
				return;

			destruct(begin(), end());
			m_Size = 0;
		}

		//IT WORKS!
		constexpr inline void pop_back() {
			if (m_Size == 0)
				return;

			destruct(end() - 1);
		}

		constexpr inline Pointer erase(ConstantPointer iterator) {
			if (m_Size == 0)
				return nullptr;

			if (iterator == end() - 1) {
				pop_back();
				return m_Iterator + (m_Size - 1);
			}
			else if (iterator == begin()) {
				if (std::destructible<Type>)
					AllocatorTraits::destroy(m_Allocator, iterator);

				std::shift_left(m_Iterator, end(), 1);
				m_Size--;
				return m_Iterator;
			}
			else {
				int Index = find_position(iterator);
				if (Index == -1)
					throw std::invalid_argument("Iterator out of bounds!");

				if (std::destructible<Type>) //Reuse destroy
					AllocatorTraits::destroy(m_Allocator, iterator);

				std::shift_left(begin() + Index, end(), 1);
				m_Size--;
				return begin() + Index;
			}
		}
		constexpr inline Pointer erase_if(ConstantPointer iterator, Predicate predicate) {
			if (m_Size == 0)
				return nullptr;
			if (predicate(*iterator))
				erase(iterator);

			return nullptr;
		}

		constexpr inline Pointer erase(ConstantPointer first, ConstantPointer last) {
			if (m_Size == 0)
				return nullptr;

			if (first == last)
				return nullptr; //??
			//std::distance //not hjere maybe 
			if (first > last)
				throw std::invalid_argument("Invalid range!");

			SizeType StartIndex = find_position(first);
			if (StartIndex == INVALID_INDEX)
				throw std::invalid_argument("Start iterator out of bounds!");

			SizeType EndIndex = find_position(last);
			if (EndIndex == INVALID_INDEX)
				throw std::invalid_argument("End interator out of bounds!");

			if (StartIndex > EndIndex)
				throw std::invalid_argument("Start iterator overlaps end interator!");


			Pointer OldEnd = end();
			destroy(m_Iterator + StartIndex, m_Iterator + EndIndex);

			std::shift_left(begin() + StartIndex, OldEnd, (EndIndex + 1) - StartIndex); //Doesnt work after refactor due to destroy modifying size

			return begin() + StartIndex;
		}
		constexpr inline Pointer erase_if(ConstantPointer first, ConstantPointer last, Predicate predicate) {
			if (m_Size == 0)
				return nullptr;

			if (first == last)
				return nullptr; //?? check ref cpp

			if (first > last)
				throw std::invalid_argument("Invalid range!");

			int StartIndex = find_index(first);
			if (StartIndex == INVALID_INDEX)
				throw std::invalid_argument("Start iterator out of bounds!");

			int EndIndex = find_index(last);
			if (EndIndex == INVALID_INDEX)
				throw std::invalid_argument("End interator out of bounds!");

			if (StartIndex > EndIndex)
				throw std::invalid_argument("Start iterator overlaps end interator!");

			int Current = StartIndex;
			for (int i = 0; i < (EndIndex + 1) - StartIndex; i++) {
				if (predicate(*(m_Iterator + Current))) {
					erase(m_Iterator + Current);
					continue;
				}
				Current++;
			}

			return nullptr; //??
		}

	public: //Iterators - ALL GOOD!
		constexpr inline Pointer begin() noexcept { return m_Iterator; }
		constexpr inline ConstantPointer begin() const noexcept { return m_Iterator; }
		constexpr inline ConstantPointer cbegin() const noexcept { return m_Iterator; }

		constexpr inline ReverseIterator rbegin() noexcept { return ReverseIterator(m_Iterator + (m_Size - 1)); }
		constexpr inline ReverseConstantIterator rbegin() const noexcept { return ReverseConstantIterator(m_Iterator + (m_Size - 1)); }
		constexpr inline ReverseConstantIterator crbegin() const noexcept { return rbegin(); }

		constexpr inline ReverseIterator rend() noexcept { return ReverseIterator(m_Iterator - 1); }
		constexpr inline ReverseConstantIterator rend() const noexcept { return ReverseConstantIterator(m_Iterator - 1); }
		constexpr inline ReverseConstantIterator crend() const noexcept { return rend(); }

		constexpr inline Pointer end() noexcept { return m_Iterator + m_Size; }
		constexpr inline ConstantPointer end() const noexcept { return m_Iterator + m_Size; }
		constexpr inline ConstantPointer cend() const noexcept { return m_Iterator + m_Size; }

	public: //Capacity
		//IT WORKS!
		constexpr inline void reserve(SizeType capacity) {
			if (m_Capacity > capacity || m_Capacity == capacity)
				return;

			if (capacity > max_size())
				throw std::length_error("Max allowed container size exceeded!");

			reallocate(capacity);
		}


		constexpr inline void shrink_to_fit() { //sus
			if (m_Capacity == m_Size)
				return;

			if (m_Size == 0 && m_Capacity > 0) {
				m_Allocator.deallocate<Type>(m_Iterator, m_Capacity);
				m_Iterator = nullptr;
				m_Capacity = 0;
				return;
			}
			else {
				Pointer NewBuffer = allocate_memory_block(m_Size);
				if (!NewBuffer)
					throw std::bad_alloc();

				SizeType DeallocationSize = m_Capacity;
				m_Capacity = m_Size;
				std::memmove(NewBuffer, m_Iterator, m_Size * sizeof(Type));
				m_Allocator.deallocate<Type>(m_Iterator, DeallocationSize);
				m_Iterator = NewBuffer;
			}
		}
		constexpr inline void swap(Container<Type>& other) noexcept {
			SizeType capacity = this->m_Capacity;
			this->m_Capacity = other.m_Capacity;
			other.m_Capacity = capacity;

			SizeType SizeType = this->m_Size;
			this->m_Size = other.m_Size;
			other.m_Size = SizeType;

			Pointer Pointer = this->m_Iterator;
			this->m_Iterator = other.m_Iterator;
			other.m_Iterator = Pointer;

			if (AllocatorTraits::propagate_on_container_swap::value)
				std::swap(m_Allocator, other.m_Allocator);
		}
		//Resize

		//All of these are good!
		constexpr inline CustomAllocator<Type> get_allocator() const noexcept { return m_Allocator; }
		constexpr inline SizeType max_size() const noexcept { return static_cast<SizeType>(pow(2, sizeof(Pointer) * 8) / sizeof(Type) - 1); }
		constexpr inline SizeType capacity() const noexcept { return m_Capacity; }
		constexpr inline SizeType size() const noexcept { return m_Size; }
		constexpr inline bool empty() const noexcept { return m_Size == 0; }
		constexpr inline bool is_null() const noexcept { return m_Iterator == nullptr; }

	private: //Memory
		//IT WORKS!
		constexpr inline void destruct_and_deallocate() {
			clear();
			deallocate_memory_block(m_Iterator, capacity(), m_Allocator);
			m_Capacity = 0;
		}

		//IT WORKS!
		constexpr inline Pointer allocate_memory_block(const SizeType capacity, Allocator& allocator) {
			//No guarantee
			Pointer NewBuffer = AllocatorTraits::allocate(allocator, sizeof(Type) * capacity);
			if (!NewBuffer)
				throw std::bad_alloc();

			return NewBuffer;
		}

		//IT WORKS! but requires reusage in some places.
		constexpr inline void deallocate_memory_block(Pointer location, const SizeType size, Allocator& allocator) {
			if (!location || size == 0)
				return;

			AllocatorTraits::deallocate(allocator, location, size);
		}

		//IT WORKS!
		//Doesnt provide guarantee
		constexpr inline void reallocate(const SizeType capacity) {
			Pointer NewBlock = allocate_memory_block(capacity, m_Allocator);
			if (m_Size > 0)
				std::memmove(NewBlock, m_Iterator, m_Size * sizeof(Type));

			if (m_Capacity > 0)
				deallocate_memory_block(m_Iterator, m_Capacity, m_Allocator);

			m_Iterator = NewBlock;
			m_Capacity = capacity;
		}

		//IT WORKS!
		//Uses allocation to allocate new memory block and deallocation to deallocate the old one. 
		constexpr inline void swap_allocator_memory(Allocator& deallocation, Allocator& allocation, const SizeType capacity) {
			Pointer NewBlock = allocate_memory_block(capacity, allocation);

			if (m_Size > 0)
				std::memmove(NewBlock, m_Iterator, m_Size * sizeof(Type)); 

			deallocate_memory_block(m_Iterator, m_Capacity, deallocation);

			m_Iterator = NewBlock;
			m_Capacity = capacity;
		}

		//IT WORKS!
		constexpr inline void allocate_and_copy_construct(SizeType capacity, SizeType size, ConstantReference value = Type()) {
			reallocate(capacity);
			construct(size, value);
		}

		//IT WORKS!
		constexpr inline void copy_assign(const Container& other) {
			if (other.size() > size()) {
				destruct(begin(), end()); //It makes sense cause other.size() is higher so they all are gonna get replaced really.
				for (unsigned int i = 0; i < other.size(); i++) {
					if (i >= m_Size)
						AllocatorTraits::construct(m_Allocator, m_Iterator + i, *(other.m_Iterator + i));
					else
						emplace(m_Iterator + i, *(other.m_Iterator + i));
				}
			}
			else {
				for (unsigned int i = 0; i < other.size(); i++) {
					if (i >= m_Size)
						AllocatorTraits::construct(m_Allocator, m_Iterator + i, *(other.m_Iterator + i));
					else
						*(m_Iterator + i) = *(other.m_Iterator + i);
				}

				if (m_Size > other.size()) // Destory leftovers.
					destruct(m_Iterator + other.size(), m_Iterator + m_Size);
			}

			m_Size = other.m_Size;
		}


		constexpr inline void uninitialized_copy_construct(const Container& other) {
			m_Size = other.m_Size;
			for (unsigned int i = 0; i < m_Size; i++)
				AllocatorTraits::construct(m_Allocator, m_Iterator + i, *(other.m_Iterator + i));
		}
		constexpr inline void uninitialized_allocate_and_move(Container&& other) {
			reallocate(other.capacity());
			for (SizeType i = 0; i < m_Size; i++)
				AllocatorTraits::construct(m_Allocator, m_Iterator + i, std::move(*(other.m_Iterator + i)));
			other.destruct_and_deallocate();
		}


		//TODO: Add one more that constructs 1 object at a spot!
		constexpr inline void construct(SizeType size, ConstantReference value) {
			m_Size = size;
			for (SizeType index = 0; index < size; index++)
				AllocatorTraits::construct(m_Allocator, m_Iterator + index, value);
		}

		//IT WORKS! but swap construct!
		constexpr inline void construct_from_list(const InitializerList& values) {
			m_Size = values.size();
			for (SizeType index = 0; index < values.size(); index++)
				AllocatorTraits::construct(m_Allocator, m_Iterator + index, *(values.begin() + index));
		}


		//Should work but try out the new swapped allocation functions!!
		//Doesnt provide strong guarantee if type can throw.
		template<class... args>
		constexpr inline void construct_and_shift(SizeType position, args&&... arguments) {
			if (position > size())
				throw std::invalid_argument("Invalid iterator access");

			if (size() + 1 == capacity()) {
				Pointer NewBuffer = allocate_memory_block(sizeof(Type), m_Allocator); //Temporary block

				AllocatorTraits::construct(m_Allocator, NewBuffer, arguments...);
				std::move_backward(m_Iterator + position, m_Iterator + size(), m_Iterator + size() + 1);
				*(m_Iterator + position) = std::move(*(NewBuffer));

				deallocate_memory_block(NewBuffer, sizeof(Type), m_Allocator); //Clean Temporary block
			}
			else {
				AllocatorTraits::construct(m_Allocator, m_Iterator + size(), arguments...);
				std::move_backward(m_Iterator + position, m_Iterator + size() + 1, m_Iterator + size() + 2);
				*(m_Iterator + position) = std::move(*(m_Iterator + size() + 1));
			}
		}

		constexpr inline void destruct(Pointer target) noexcept {
			if (!target)
				return;

			if (std::destructible<Type>) // Will pass check even if fundemental
				AllocatorTraits::destroy(m_Allocator, target);

			m_Size--;
		}
		constexpr inline void destruct(ConstantPointer target) noexcept {
			if (!target)
				return;

			if (std::destructible<Type>) // Will pass check even if fundemental
				AllocatorTraits::destroy(m_Allocator, target);

			m_Size--;
		}

		//IT WORKS!
		constexpr inline void destruct(Pointer first, Pointer last) noexcept {
			if (!first || !last)
				return;

			if (first > last)
				return;

			if (first == last)
				destruct(first);

			if (std::destructible<Type>) { // Will pass check even if fundemental
				for (Pointer i = last - 1; i >= first; i--) {
					AllocatorTraits::destroy(m_Allocator, i);
					m_Size--;
				}
			}
		}

		//IT WORKS!
		constexpr inline void wipe() noexcept {
			m_Iterator = nullptr;
			m_Size = 0;
			m_Capacity = 0;
		}

	private: //Helpers
		constexpr inline int find_position(Pointer iterator) const noexcept {
			if (iterator == begin())
				return 0;
			if (iterator == end())
				return static_cast<int>(m_Size);  //Scary

			for (int i = 0; i < m_Size; i++) {
				if (m_Iterator + i == iterator)
					return i;
			}
			return INVALID_INDEX;
		}
		constexpr inline int find_position(ConstantPointer iterator) const noexcept {
			if (iterator == begin())
				return 0;
			if (iterator == end())
				return static_cast<int>(m_Size); //Scary

			for (int i = 0; i < m_Size; i++) {
				if (m_Iterator + i == iterator)
					return i;
			}
			return INVALID_INDEX;
		}

	private:
		Pointer m_Iterator = nullptr;
		SizeType m_Capacity = 0;
		SizeType m_Size = 0;
		Allocator m_Allocator;
	};



	namespace {
		//Operators


	}

	namespace pmr {
		template<class T>
		using Container = Marigold::Container<T, std::pmr::polymorphic_allocator<T>>;
	}
}

#endif // !CONTAINER_H