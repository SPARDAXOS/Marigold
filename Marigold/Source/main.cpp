#include "iostream"
#include "Container.h"
#include "Allocator.h"



struct Element {

	Element() {
		std::cout << "ctor " << number << std::endl;
	}
	Element(int value) 
		:	number(value)
	{
		std::cout << "ctor " << number << std::endl;
	}
	~Element() {
		std::cout << "dtor " << number << std::endl;
	}


	Element(const Element& other) {
		std::cout << "copy ctor " << number << std::endl;
		*this = other;
	}
	Element& operator=(const Element& other) {
		std::cout << "copy assignment ctor " << number << std::endl;
		if (this == &other)
			return *this;
		else {
			return *this;
		}
	}

	Element(Element&& other) {
		std::cout << "move ctor " << number << std::endl;
		*this = std::move(other);
	}
	Element& operator=(Element&& other) {
		std::cout << "move assignment ctor " << number << std::endl;
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


















	Element element;
	Marigold::Container<Element> container;

	container.reserve(10);
	container.emplace_back(element);

	if (container.size() > 0)
		container.clear();

	container.push_back(element);
	container.insert(&container[1], element);

	container.shrink_to_fit();
	if (container.capacity() == 2)
		container.erase(&container[0]);

















	//Marigold::Container<Element> container;
	container.push_back({ 1 });
	container.push_back({ 1 });
	container.push_back({ 1 });
	
	std::cout << "/////////Container Process Start/////////" << std::endl;
	Marigold::Container<Element> container2(std::move(container), allocator);
	std::cout << "/////////Container Process End/////////" << std::endl;
	
	std::vector<Element> vector;
	vector.push_back({ 1 });
	vector.push_back({ 1 });
	vector.push_back({ 1 });
	
	std::cout << "/////////Vector Process Start/////////" << std::endl;
	std::vector<Element> vector2(std::move(vector));
	std::cout << "/////////Vector Process End/////////" << std::endl;




	//Copy test!
	//std::cout << "/////////Container Fodder Start/////////" << std::endl;
	//Marigold::Container<Element> container;
	//container.push_back({ 1 });
	//container.push_back({ 1 });
	//container.push_back({ 1 });
	//
	//
	//Marigold::Container<Element> container2;
	//container2.push_back({ 2 });
	//container2.push_back({ 2 });
	//container2.push_back({ 2 });
	//container2.push_back({ 2 });
	//container2.push_back({ 2 });
	//
	//std::cout << "/////////Container Fodder End/////////" << std::endl;
	//container2 = container;
	//
	//std::cout << "/////////Vector Fodder Start/////////" << std::endl;
	//std::vector<Element> vector;
	//vector.push_back({ 1 });
	//vector.push_back({ 1 });
	//vector.push_back({ 1 });
	//
	//
	//std::vector<Element> vector2;
	//vector2.push_back({ 2 });
	//vector2.push_back({ 2 });
	//vector2.push_back({ 2 });
	//vector2.push_back({ 2 });
	//vector2.push_back({ 2 });
	//
	//std::cout << "/////////Vector Fodder End/////////" << std::endl;
	//vector2 = vector;
	

	//It works but test it out again with the normal vector and check how the ctor/dtor behaves!
	
	//I dont understand how the standard vector calls normal ctor when copying over... It makes since that mine calls the copy ctor. Behavior even makes sense!
	//It should construct them as if they are new. They are just being copied over so no reconstruction.

	std::cout << memory << std::endl;
	return 0;
}