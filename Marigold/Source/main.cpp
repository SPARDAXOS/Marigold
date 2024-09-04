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
			this->number = other.number;
			return *this;
		}
	}

	Element(Element&& other) noexcept {
		std::cout << "move ctor " << number << std::endl;
		*this = std::move(other);
	}
	Element& operator=(Element&& other) noexcept {
		std::cout << "move assignment ctor " << number << std::endl;
		if (this == &other)
			return *this;
		else {
			this->number = std::move(other.number);
			return *this;
		}
	}


	bool operator==(const Element& other) const noexcept {
		if (number == other.number)
			return true;
		return false;
	}
	bool operator!=(const Element& other) const noexcept {
		return !(*this == other);
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
	Element element1{ 1 };
	Element element2{ 2 };
	Element element3{ 3 };
	Element element4{ 4 };
	Element element5{ 5 };
	//std::vector<Element> vectorTest;
	//vectorTest.reserve(10);
	//vectorTest.emplace(vectorTest.begin() + 5, element);

	std::initializer_list<Element> list = { {2}, {2}, {2}, {2}, {2} };

	//Emplace test
	Marigold::Container<Element> container;
	container.reserve(5);
	container.emplace(container.end(), element1);
	//container.insert(container.begin(), 5, element5);

	container.emplace(container.end(), element2);
	container.emplace(container.end(), element3);
	container.emplace(container.end(), element4);

	//Marigold::erase_if(container, [](Element& element) {
	//	if (element.number == 1)
	//		return true; 
	//	return false;
	//	});


	//container.emplace(container.begin() + 1, element4);
	Marigold::Container<Element> container2;
	container2.emplace_back(element);
	container2.emplace_back(element);


	container2.assign(container.begin(), container.end());
	container2.insert(container2.end(), 5, element5);
	container2.insert(container2.end(), container.begin(), container.end());

	Marigold::swap(container, container2);
	container.swap(container2);

	container = container2;


	//Results: There is difference between marigold and standard when it comes to shifting elements in emplace.
	// The standard calls dtor while marigold calls 3 more move assignments but no dtor.
	//std::vector<Element> vector;
	//vector.reserve(10);
	//vector.emplace(vector.end(), element1);
	//vector.emplace(vector.end(), element2);
	//vector.emplace(vector.end(), element3);
	//vector.emplace(vector.begin() + 1, element4);
	//vector.emplace(vector.end(), element3);

	//Marigold::Container<Element> container2;
	container2.emplace_back(element); 
	container2.emplace_back(element);
	container2.emplace_back(element);
	container2.at(0);
	std::cout << "/////////Container Process Start/////////" << std::endl;
	container2 = std::move(container);
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