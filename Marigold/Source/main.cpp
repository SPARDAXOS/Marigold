#include "iostream"
#include "Container.h"
#include "Allocator.h"



struct Element {

	Element() {
		std::cout << "ctor" << std::endl;
	}
	Element(int value) 
		:	number(value)
	{
		std::cout << "ctor" << std::endl;
	}
	~Element() {
		std::cout << "dtor" << std::endl;
	}


	Element(const Element& other) {
		std::cout << "copy ctor" << std::endl;
		*this = other;
	}
	Element& operator=(const Element& other) {
		std::cout << "copy assignment ctor" << std::endl;
		if (this == &other)
			return *this;
		else {
			return *this;
		}
	}

	Element(Element&& other) {
		std::cout << "move ctor" << std::endl;
		*this = std::move(other);
	}
	Element& operator=(Element&& other) {
		std::cout << "move assignment ctor" << std::endl;
		if (this == &other)
			return *this;
		else {
			return *this;
		}
	}



	int number = 1;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

	//Test every single special member function
	CustomAllocator<int> allocator;
	int* memory = allocator.allocate(12);
	//int value[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	//Marigold::Container<int> container(&value[0], &value[9] + 1);

	//std::size_t number = 10;



	//Marigold::Container<Element> container;
	//container.push_back({ 1 });
	//container.push_back({ 1 });
	//container.push_back({ 1 });
	//
	//Marigold::Container<Element> container2(std::move(container));










	//Copy test!
	Marigold::Container<Element> container;
	container.push_back({ 1 });
	container.push_back({ 1 });
	container.push_back({ 1 });
	container.push_back({ 1 });
	container.push_back({ 1 });
	
	
	
	Marigold::Container<Element> container2;
	container2.push_back({ 2 });
	container2.push_back({ 2 });
	container2.push_back({ 2 });
	
	std::cout << "sbdiabskdbsakldblsiajdölkasjdölksada" << std::endl;
	container2 = container;
	

	//It works but test it out again with the normal vector and check how the ctor/dtor behaves!
	
	//I dont understand how the standard vector calls normal ctor when copying over... It makes since that mine calls the copy ctor. Behavior even makes sense!
	//It should construct them as if they are new. They are just being copied over so no reconstruction.

	std::cout << memory << std::endl;
	return 0;
}