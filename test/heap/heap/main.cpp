
#include <wawo.h>


struct MyNode {
	int v;

	MyNode() :v(0)
	{
	}

	MyNode(int v_) : v(v_)
	{
	}

	MyNode(MyNode const& r) :
		v(r.v)
	{
	}

	MyNode& operator = (MyNode const & r)
	{
		v = r.v;
		return *this;
	}

	~MyNode()
	{
	}
};

inline static bool operator < (MyNode const& l, MyNode const& r) {
	return l.v < r.v;
}

inline static bool operator > (MyNode const&l, MyNode const& r) {
	return l.v > r.v;
}

template <class HeapT, class NodeT>
void test_heap(wawo::u32_t const& max_v , bool increr_mode ) {

	std::srand(0);
	wawo::u64_t begin = wawo::time::curr_nanoseconds();

	unsigned long _b = 1;
	if (!increr_mode) {
		_b = max_v ;
	}

	for (unsigned long cur_v = _b; cur_v <= max_v; ++cur_v) {
		unsigned long _cur_v = cur_v;
		HeapT* h = new HeapT();

		while (_cur_v--) {
			NodeT* bn = new NodeT();
			int r = std::rand();
			bn->data = MyNode(r);
			h->push(bn);
		}

		while (!h->empty()) {
			NodeT* n = h->front();
			//printf("%d ", n->data.v);
			h->pop();
			delete n;
		}

		delete h;
	}

	wawo::u64_t now = wawo::time::curr_nanoseconds();
	printf("total cost: %f ms\n", (now - begin)/1000000.0 );
}

typedef wawo::binary_heap<MyNode> BinH;

template <>
void test_heap<BinH, MyNode>(wawo::u32_t const& max_v , bool increr_mode ) {
	std::srand(0);
	wawo::u64_t begin = wawo::time::curr_nanoseconds();

	unsigned long _b = 1;
	if (!increr_mode) {
		_b = max_v ;
	}

	for (unsigned long cur_v = _b; cur_v <= max_v; ++cur_v) {
		unsigned long _cur_v = cur_v;
		BinH h = BinH(cur_v);

		while (_cur_v--) {
			MyNode bn;
			bn.v = std::rand();
			h.push(bn);
		}
		
		while (!h.empty()) {
			MyNode& n = h.front();
			//printf("%d ", n.v);
			h.pop();
		}
	}

	wawo::u64_t now = wawo::time::curr_nanoseconds();
	printf("total cost: %f ms\n", (now - begin) / 1000000.0);
}

template <>
void test_heap<std::vector<MyNode>,MyNode>(wawo::u32_t const& max_v, bool increr_mode) {

	std::srand(0);
	wawo::u64_t begin = wawo::time::curr_nanoseconds();

	unsigned long _b = 1;
	if (!increr_mode) {
		_b = max_v;
	}

	for (unsigned long cur_v = _b; cur_v <= max_v; ++cur_v) {
		unsigned long _cur_v = cur_v;
		std::vector<MyNode> hvec;
		std::make_heap(hvec.begin(), hvec.end(), std::greater<>());

		while (_cur_v--) {
			MyNode bn;
			bn.v = std::rand();
			hvec.push_back(bn);
			std::push_heap(hvec.begin(), hvec.end(), std::greater<>());
		}
		
		while (!hvec.empty()) {
			std::pop_heap(hvec.begin(), hvec.end(), std::greater<>());
			MyNode& n = hvec.back();
			//printf("%d ", n.v);
			hvec.pop_back();
		}
	}

	wawo::u64_t now = wawo::time::curr_nanoseconds();
	printf("total cost: %f ms\n", (now - begin) / 1000000.0);
}

int main(int argc, char** argv) {

	const unsigned int size = 50000000;

	test_heap<std::vector<MyNode>, MyNode>(size, false);
	test_heap<BinH, MyNode>(size, false);

	/*
	typedef wawo::binomial_heap<MyNode> BinoH;
	typedef wawo::binomial_node<MyNode> BinoNode;
	test_heap<BinoH, BinoNode>(size,false);

	typedef wawo::fibonacci_heap<MyNode> FibH;
	typedef wawo::fibonacci_node<MyNode> FibNode;
	test_heap<FibH, FibNode>(size, false);
	*/
	return 0;
}