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
			for (SizeType index = 0; index < list.size(); index++)
				construct(begin() + index, *(list.begin() + index));
		}

		//Copy Semantics
		constexpr Container(const Container& other)
			: m_Allocator(AllocatorTraits::select_on_container_copy_construction(other.m_Allocator))
		{
			if (!other.m_Data)
				return;

			reserve(other.m_Size);
			uninitialized_copy_construct(other);
		}
		constexpr Container(const Container& other, const Allocator& allocator)
			: m_Allocator(allocator)
		{
			if (!other.m_Data)
				return;

			reserve(other.m_Size);
			uninitialized_copy_construct(other);
		}
		Container& operator=(const Container& other) noexcept {
			if (this == &other)
				return *this;

			if (other.m_Data) {
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

				if (other.size() > capacity())
					reserve(other.capacity());
				copy_assign(other);
				return *this;
			}

			clear();
			return *this;
		}

		//Move Semantics
		constexpr Container(Container&& other) noexcept //Fixed: IT WORKS! Not sure. Im not moving the elements. check vector behavior. Might be fine to leave it
			: m_Allocator(std::move(other.m_Allocator)), m_Data(other.m_Data), m_Size(other.m_Size), m_Capacity(other.m_Capacity)
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
				m_Data = other.m_Data;//? wot hérew warning
				other.wipe();
			}
		}
		Container& operator=(Container&& other) noexcept {
			if (this == &other)
				return *this;

			if constexpr (AllocatorTraits::propagate_on_container_move_assignment::value) {
				destruct_and_deallocate();
				this->m_Allocator = other.get_allocator();
				this->m_Data = other.m_Data;
				this->m_Size = other.m_Size;
				this->m_Capacity = other.m_Capacity;
				other.wipe();
			}
			else if (!AllocatorTraits::propagate_on_container_move_assignment::value && this->m_Allocator == other.get_allocator()) {
				destruct_and_deallocate();
				this->m_Data = other.m_Data;
				this->m_Size = other.m_Size;
				this->m_Capacity = other.m_Capacity;
				other.wipe();
			}
			else {
				clear();
				if (other.size() > capacity())
					reserve(other.capacity());

				for (SizeType i = 0; i < other.size(); i++) {
					AllocatorTraits::construct(m_Allocator, m_Data + i, std::move(*(other.m_Data + i)));
					m_Size++;
				}

				other.destruct_and_deallocate();
				other.wipe();
			}

			return *this;
		}

		//Dtor //IT WORKS!
		~Container() {
			destruct_and_deallocate();
		}

		//IT WORKS!
		constexpr Container& operator=(InitializerList ilist) {
			clear();
			if (ilist.size() > capacity())
				reserve(ilist.size());

			m_Size = ilist.size();
			for (SizeType i = 0; i < ilist.size(); i++)
				AllocatorTraits::construct(m_Allocator, m_Data + i, *(ilist.begin() + i));

			return *this;
		}

	public: //Access - ALL GOOD!
		constexpr inline Reference at(SizeType index) {
			if (index >= m_Size)
				throw std::out_of_range("Access Violation - " + index);

			return m_Data[index];
		}
		constexpr inline ConstantReference at(SizeType index) const {
			if (index >= m_Size)
				throw std::out_of_range("Access Violation - " + index);

			return m_Data[index];
		}

		constexpr inline Pointer data() noexcept { 
			if (size() <= 0)
				return nullptr;
			return m_Data; 
		}
		constexpr inline ConstantPointer data() const noexcept { 
			if (size() <= 0)
				return nullptr;
			return m_Data; 
		}

		constexpr inline Reference front() noexcept { return m_Data[0]; }
		constexpr inline ConstantReference front() const noexcept { return m_Data[0]; }

		constexpr inline Reference back() noexcept { return m_Data[m_Size - 1]; }
		constexpr inline ConstantReference back() const noexcept { return m_Data[m_Size - 1]; }

		constexpr inline Reference operator[](SizeType index) noexcept { 
			assert(index < size() && "Index out of range");
			return m_Data[index]; 
		} 
		constexpr inline ConstantReference operator[](SizeType index) const noexcept { 
			assert(index < size() && "Index out of range");
			return m_Data[index]; 
		} 

	public: //Insertion - ALL GOOD!
		constexpr inline void push_back(ConstantReference value) {
			emplace_back(value);
		}
		constexpr inline void push_back(Type&& value) {
			emplace_back(std::move(value));
		}
		template<class... args>
		constexpr inline Pointer emplace(ConstantPointer address, args&&... arguments) { 
			assert(address <= end() && "Vector's argument out of range.");

			SizeType IndexPosition = 0;
			if (address == end())
				IndexPosition = size();
			else
				IndexPosition = std::distance<ConstantPointer>(begin(), address);

			if (m_Capacity == 0)
				reserve(1);
			else if (m_Size == m_Capacity)
				reserve(capacity() * REALLOCATION_FACTOR);

			if (m_Data + IndexPosition == end()) {
				try {
					AllocatorTraits::construct(m_Allocator, m_Data + size(), std::forward<args>(arguments)...);
				}
				catch (...) {
					destruct(m_Data + size());
					throw;
				}
			}
			else
				construct_and_shift(IndexPosition, std::forward<args>(arguments)...);

			m_Size++;
			return m_Data + IndexPosition;
		}
		template<class... args>
		constexpr inline Reference emplace_back(args&&... arguments) {
			emplace(end(), std::forward<args>(arguments)...);
			return *(m_Data + (m_Size - 1));
		}
		constexpr inline Pointer insert(ConstantPointer position, ConstantReference value) {
			return emplace(position, value);
		}
		constexpr inline Pointer insert(ConstantPointer position, Type&& value) {
			return emplace(position, std::move(value));
		}
		constexpr inline Pointer insert(ConstantPointer position, SizeType count, ConstantReference value) {
			assert(position <= end() && "Vector's argument out of range.");
			if (count == 0 || !position && size() != 0)
				return (Pointer)position;


			if (position == begin() || !position && size() == 0) {
				while (size() + count > capacity()) {
					if (capacity() == 0)
						reserve(1);
					else
						reserve(capacity() * REALLOCATION_FACTOR);
				}

				std::shift_right(begin(), begin() + capacity(), count);
				for (SizeType i = 0; i < count; i++)
					construct(begin() + i, value);

				return begin();
			}
			else if (position == end()) {
				while (size() + count > capacity()) {
					if (capacity() == 0)
						reserve(1);
					else
						reserve(capacity() * REALLOCATION_FACTOR);
				}

				for (SizeType i = 0; i < count; i++)
					emplace_back(value);
				return end();
			}
			else {
				SizeType index = std::distance<ConstantPointer>(begin(), position);
				while (size() + count > capacity()) {
					if (capacity() == 0)
						reserve(1);
					else
						reserve(capacity() * REALLOCATION_FACTOR);
				}

				std::shift_right(begin() + index, begin() + capacity(), count);
				for (SizeType i = 0; i < count; i++)
					construct(begin() + index + i, value);

				return begin() + index;
			}
		}
		template<IsPointer InputIterator>
		constexpr Pointer insert(ConstantPointer position, InputIterator first, InputIterator last) {
			assert(position <= end() && "Vector's argument out of range.");
			if (first == last)
				return (Pointer)position;

			SizeType index = std::distance<ConstantPointer>(begin(), position);
			SizeType targetRangeIndex = std::distance(first, last);
			for (SizeType i = 0; i < targetRangeIndex; i++)
				emplace(begin() + index, *(first + i));

			return begin() + index;
		}
		constexpr Pointer insert(ConstantPointer position, InitializerList ilist) {
			assert(position <= end() && "Vector's argument out of range.");
			if (ilist.size() == 0)
				return (Pointer)position;

			SizeType index = std::distance<ConstantPointer>(begin(), position);
			for (SizeType i = 0; i < ilist.size(); i++)
				emplace(begin() + index, *(ilist.begin() + i));

			return begin() + index;
		}
		constexpr void assign(SizeType count, ConstantReference value) {
			if (size() > 0)
				clear();

			if (count > capacity())
				reserve(count);

			for (SizeType i = 0; i < count; i++)
				construct(begin() + i, value);
		}

		//Behavior might defer slightly from standard due to documentation errors.
		template<typename InputIt>
		constexpr void assign(InputIt first, InputIt last) {
			if (size() > 0)
				clear();

			SizeType size = std::distance(first, last);
			if (size > capacity())
				reserve(size);

			for (SizeType index = 0; index < size; index++)
				construct(begin() + index, *(first + index));
		}
		constexpr void assign(InitializerList list) { 
			if (size() > 0)
				clear();

			if (list.size() > capacity())
				reserve(list.size());

			for (SizeType i = 0; i < list.size(); i++)
				construct(begin() + i, *(list.begin() + i));
		}

	public: //Removal  - ALL GOOD!
		constexpr inline void clear() noexcept {
			if (m_Size == 0)
				return;

			destruct(begin(), end());
			m_Size = 0;
		}
		constexpr inline void pop_back() {
			if (m_Size == 0)
				return;

			destruct(end() - 1);
		}
		constexpr inline Pointer erase(ConstantPointer iterator) {
			assert(iterator < end() && "Vector subscript out of range");

			if (iterator == end() - 1) {
				pop_back();
				return end();
			}
			else if (iterator == begin()) {
				destruct(iterator);
				std::shift_left(begin(), end(), 1);
				return  begin();
			}
			else {
				SizeType index = std::distance<ConstantPointer>(begin(), iterator);
				destruct(iterator);
				std::shift_left(begin() + index, end(), 1);
				return begin() + index;
			}
		}
		constexpr inline Pointer erase(ConstantPointer first, ConstantPointer last) {
			if (size() == 0)
				return (Pointer)last;

			assert(first <= last && "First argument is out of range");
			assert(last <= last && "Last argument is out of range");
			assert(first <= last && "First argument is smaller than last - Invalid input");

			bool lastEqualsEnd = (last == end());
			SizeType firstIndex = std::distance<ConstantPointer>(begin(), first);
			SizeType lastIndex = std::distance<ConstantPointer>(begin(), last);

			Pointer endptr = end();
			destruct(begin() + firstIndex, begin() + lastIndex);
			std::move(begin() + lastIndex, endptr, begin() + firstIndex);

			if (lastEqualsEnd)
				return end();
			return begin() + firstIndex;
		}

	public: //Iterators - ALL GOOD!
		constexpr inline Pointer begin() noexcept { return m_Data; }
		constexpr inline ConstantPointer begin() const noexcept { return m_Data; }
		constexpr inline ConstantPointer cbegin() const noexcept { return m_Data; }

		constexpr inline ReverseIterator rbegin() noexcept { return ReverseIterator(m_Data + (m_Size - 1)); }
		constexpr inline ReverseConstantIterator rbegin() const noexcept { return ReverseConstantIterator(m_Data + (m_Size - 1)); }
		constexpr inline ReverseConstantIterator crbegin() const noexcept { return rbegin(); }

		constexpr inline ReverseIterator rend() noexcept { return ReverseIterator(m_Data - 1); }
		constexpr inline ReverseConstantIterator rend() const noexcept { return ReverseConstantIterator(m_Data - 1); }
		constexpr inline ReverseConstantIterator crend() const noexcept { return rend(); }

		constexpr inline Pointer end() noexcept { return m_Data + m_Size; }
		constexpr inline ConstantPointer end() const noexcept { return m_Data + m_Size; }
		constexpr inline ConstantPointer cend() const noexcept { return m_Data + m_Size; }

	public: //Capacity - ALL GOOD!
		constexpr inline void reserve(SizeType capacity) {
			if (m_Capacity > capacity || m_Capacity == capacity)
				return;

			if (capacity > max_size())
				throw std::length_error("Max allowed container size exceeded!");

			reallocate(capacity);
		}
		constexpr inline void shrink_to_fit() {
			if (m_Capacity == m_Size)
				return;

			if (size() == 0) {
				deallocate_memory_block(m_Data, capacity(), m_Allocator);
				m_Capacity = 0;
				m_Data = nullptr;
			}
			else
				reallocate(size());
		}
		constexpr inline void swap(Container<Type>& other) noexcept {
			if (this == &other)
				return;

			if constexpr (AllocatorTraits::propagate_on_container_swap::value || AllocatorTraits::is_always_equal::value)
				std::swap(m_Allocator, other.m_Allocator);

			std::swap(m_Data, other.m_Data);
			std::swap(m_Capacity, other.m_Capacity);
			std::swap(m_Size, other.m_Size);
		}
		constexpr void resize(SizeType count) {
			if (count == size())
				return;

			if (count < size()) {
				while (count < size())
					pop_back();
			}
			else {
				if (count > capacity())
					reserve(count);
				while (count > size())
					emplace_back(Type());
			}
		}
		constexpr void resize(SizeType count, ConstantReference value) {
			if (count == size())
				return;

			if (count < size()) {
				while (count < size())
					pop_back();
			}
			else {
				if (count > capacity())
					reserve(count);
				while (count > size())
					emplace_back(value);
			}
		}

		constexpr inline CustomAllocator<Type> get_allocator() const noexcept { return m_Allocator; }
		constexpr inline SizeType max_size() const noexcept { return static_cast<SizeType>(pow(2, sizeof(Pointer) * 8) / sizeof(Type) - 1); }
		constexpr inline SizeType capacity() const noexcept { return m_Capacity; }
		constexpr inline SizeType size() const noexcept { return m_Size; }
		constexpr inline bool empty() const noexcept { return m_Size == 0; }
		constexpr inline bool is_null() const noexcept { return m_Data == nullptr; }

	private: //Memory
		//IT WORKS!
		constexpr inline void destruct_and_deallocate() {
			clear();
			deallocate_memory_block(m_Data, capacity(), m_Allocator);
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
				std::memmove(NewBlock, m_Data, m_Size * sizeof(Type));

			if (m_Capacity > 0)
				deallocate_memory_block(m_Data, m_Capacity, m_Allocator);

			m_Data = NewBlock;
			m_Capacity = capacity;
		}

		//IT WORKS!
		//Uses allocation to allocate new memory block and deallocation to deallocate the old one. 
		constexpr inline void swap_allocator_memory(Allocator& deallocation, Allocator& allocation, const SizeType capacity) {
			Pointer NewBlock = allocate_memory_block(capacity, allocation);

			if (m_Size > 0)
				std::memmove(NewBlock, m_Data, m_Size * sizeof(Type)); 

			deallocate_memory_block(m_Data, m_Capacity, deallocation);

			m_Data = NewBlock;
			m_Capacity = capacity;
		}

		//IT WORKS!
		constexpr inline void allocate_and_copy_construct(SizeType capacity, SizeType size, ConstantReference value = Type()) {
			reserve(capacity);
			construct(size, value);
		}

		//IT WORKS!
		constexpr inline void copy_assign(const Container& other) {
			if (other.size() > size()) {
				destruct(begin(), end()); //It makes sense cause other.size() is higher so they all are gonna get replaced really.
				for (unsigned int i = 0; i < other.size(); i++) {
					if (i >= m_Size)
						AllocatorTraits::construct(m_Allocator, m_Data + i, *(other.m_Data + i));
					else
						emplace(m_Data + i, *(other.m_Data + i));
				}
			}
			else {
				for (unsigned int i = 0; i < other.size(); i++) {
					if (i >= m_Size)
						AllocatorTraits::construct(m_Allocator, m_Data + i, *(other.m_Data + i));
					else
						*(m_Data + i) = *(other.m_Data + i);
				}

				if (m_Size > other.size()) // Destory leftovers.
					destruct(m_Data + other.size(), m_Data + m_Size);
			}

			m_Size = other.m_Size;
		}


		//Test those along with the ctors and both this and that section should be done.
		constexpr inline void uninitialized_copy_construct(const Container& other) {
			m_Size = other.size();
			for (unsigned int i = 0; i < size(); i++)
				AllocatorTraits::construct(m_Allocator, begin() + i, *(other.begin() + i));
		}
		constexpr inline void uninitialized_allocate_and_move(Container&& other) {
			reserve(other.capacity()); //Careful of this.
			for (SizeType i = 0; i < size(); i++)
				AllocatorTraits::construct(m_Allocator, begin() + i, std::move(*(other.begin() + i)));
			other.destruct_and_deallocate();
		}



		


		//IT WORKS!
		constexpr inline void construct(ConstantPointer position, ConstantReference value) {
			if (!position)
				return;

			AllocatorTraits::construct(m_Allocator, position, value);
			m_Size++;
		}

		//IT WORKS!
		//Doesnt provide strong guarantee if type can throw.
		template<class... args>
		constexpr inline void construct_and_shift(SizeType position, args&&... arguments) {
			if (position > size())
				throw std::invalid_argument("Invalid iterator access");

			if (size() + 1 == capacity()) {
				Pointer NewBuffer = allocate_memory_block(sizeof(Type), m_Allocator); //Temporary block

				AllocatorTraits::construct(m_Allocator, NewBuffer, arguments...);
				std::move_backward(m_Data + position, m_Data + size(), m_Data + size() + 1);
				*(m_Data + position) = std::move(*(NewBuffer));

				deallocate_memory_block(NewBuffer, sizeof(Type), m_Allocator); //Clean Temporary block
			}
			else {
				AllocatorTraits::construct(m_Allocator, m_Data + size(), arguments...);
				std::move_backward(m_Data + position, m_Data + size() + 1, m_Data + size() + 2);
				*(m_Data + position) = std::move(*(m_Data + size() + 1));
			}
		}

		//IT WORKS!
		constexpr inline void destruct(Pointer target) noexcept {
			if (!target)
				return;

			if (std::destructible<Type>)
				AllocatorTraits::destroy(m_Allocator, target);

			m_Size--;
		}

		//IT WORKS!
		constexpr inline void destruct(ConstantPointer target) noexcept {
			if (!target)
				return;

			if (std::destructible<Type>)
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

			if (std::destructible<Type>) {
				for (Pointer i = last - 1; i >= first; i--) {
					AllocatorTraits::destroy(m_Allocator, i);
					m_Size--;
				}
			}
		}

		//IT WORKS!
		constexpr inline void wipe() noexcept {
			m_Data = nullptr;
			m_Size = 0;
			m_Capacity = 0;
		}

	private:
		Pointer m_Data = nullptr;
		SizeType m_Capacity = 0;
		SizeType m_Size = 0;
		Allocator m_Allocator;
	};


	//Swap - IT WORKS!
	template<class Type, class Allocator>
	constexpr void swap(Container<Type, Allocator>& lhs, Container<Type, Allocator>& rhs) noexcept {
		lhs.swap(rhs);
	}

	//Both work but the erase is kinda sus!
	template<typename Type, typename Allocator, typename Val = Type>
	constexpr Container<Type, Allocator>::SizeType erase(Container<Type, Allocator>& container, const Val& value) {
		auto it = std::remove(container.begin(), container.end(), value);
		auto r = std::distance(it, container.end());
		container.erase(it, container.end());
		return r;
	}

	template<class Type, class Allocator, class Predicate>
	constexpr Container<Type, Allocator>::SizeType erase_if(Container<Type, Allocator>& container, Predicate predicate) {
		auto it = std::remove_if(container.begin(), container.end(), predicate);
		auto r = std::distance(it, container.end());
		container.erase(it, container.end());
		return r;
	}



	//Operators



	namespace pmr {
		template<class T>
		using Container = Marigold::Container<T, std::pmr::polymorphic_allocator<T>>;
	}
}

#endif // !CONTAINER_H