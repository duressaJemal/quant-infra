#include <iostream>
#include <typeinfo>
#include <cstdlib>

// Custom trick to cleanly print type names in the terminal (GCC/Clang/MSVC)
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
template <typename T>
void printType(const char* label) {
    int status;
    // Uses the standard __cxa_demangle recognized by Apple Clang and GCC
    char* demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
    std::cout << label << ": " << (status == 0 ? demangled : typeid(T).name()) << "\n";
    std::free(demangled);
}
#else
template <typename T>
void printType(const char* label) {
    std::cout << label << ": " << typeid(T).name() << "\n";
}
#endif

// ============================================================================
// 1. CORE TYPES: ENUMS AND SELF-DESCRIBING ORDERS
// ============================================================================
enum Side { BUY, SELL };

template <int OrderId, int OrderSide, int Price, int Qty>
struct Order {
    static constexpr int orderId = OrderId;
    static constexpr int side    = OrderSide;
    static constexpr int price   = Price;
    static constexpr int qty     = Qty;
};

// ============================================================================
// 2. GENERIC COMPILE-TIME FIFO QUEUE
// ============================================================================
template <typename... T>
struct Queue {
    static constexpr int size = sizeof...(T);
};

// --- QueuePush ---
template <typename Q, typename T>
struct QueuePush;

template <typename T, typename... QElem>
struct QueuePush<Queue<QElem...>, T> {
    using type = Queue<QElem..., T>;
};

template <typename Q, typename T>
using QueuePush_t = typename QueuePush<Q, T>::type;

// --- QueuePop ---
template <typename Q>
struct QueuePop;

template <typename T0, typename... T1toN>
struct QueuePop<Queue<T0, T1toN...>> {
    using type = Queue<T1toN...>;
};

template <typename Q>
using QueuePop_t = typename QueuePop<Q>::type;

// --- QueueMerge ---
template<typename Q1, typename Q2>
struct QueueMerge;

template<typename... Q1, typename... Q2>
struct QueueMerge<Queue<Q1...>, Queue<Q2...>> {
    using type = Queue<Q1..., Q2...>;
};

template<typename Q1, typename Q2>
using QueueMerge_t = typename QueueMerge<Q1, Q2>::type;

// ============================================================================
// 3. RECURSIVE ALGORITHM: REMOVE ORDER FROM QUEUE
// ============================================================================
template<int OrderId, typename Queue>
struct RemoveOrderFromQueue;

// Base Case: Empty Queue
template<int OrderId>
struct RemoveOrderFromQueue<OrderId, Queue<>> {
    using type = Queue<>;
};

// Recursive Loop Case
template<int OrderId, typename O1, typename... O2toN>
struct RemoveOrderFromQueue<OrderId, Queue<O1, O2toN...>> {
    using remainingType = typename RemoveOrderFromQueue<OrderId, Queue<O2toN...>>::type;

    using type = std::conditional_t<
        O1::orderId == OrderId,
        remainingType,
        QueueMerge_t<Queue<O1>, remainingType>
    >;
};

template<int OrderId, typename Queue>
using RemoveOrderFromQueue_t = typename RemoveOrderFromQueue<OrderId, Queue>::type;

// ============================================================================
// 4. PRICE LEVEL ABSTRACTION (CONTAINER)
// ============================================================================
template <int Price, typename OrderQueue>
struct Level {
    static constexpr int price = Price;
    using orderQueue = OrderQueue;
};

template<int Price>
using EmptyLevel = Level<Price, Queue<>>;

// ============================================================================
// 5. PRICE LEVEL ACTIONS (ADD & REMOVE) WITH GLOBAL SCOPE DISAMBIGUATION (::)
// ============================================================================

// --- Add Order to Level ---
template <typename Level, typename Order>
struct AddOrderToLevel {
    using _newQueue = QueuePush_t<typename Level::orderQueue, Order>;
    using type = ::Level<Level::price, _newQueue>; // Uses :: to look globally for the blueprint
};

template<typename Level, typename Order>
using AddOrderToLevel_t = typename AddOrderToLevel<Level, Order>::type;

// --- Remove Order from Level ---
template<int OrderId, typename Level>
struct RemoveOrderFromLevel {
    using type = ::Level<
        Level::price,
        RemoveOrderFromQueue_t<OrderId, typename Level::orderQueue>
    >;
};

template<int OrderId, typename Level>
using RemoveOrderFromLevel_t = typename RemoveOrderFromLevel<OrderId, Level>::type;

// ============================================================================
// 6. RUNNABLE VERIFICATION (MAIN)
// ============================================================================
int main() {
    std::cout << "--- Forging Orders ---\n";
    using O1 = Order<101, BUY, 50, 10>;
    using O2 = Order<102, BUY, 50, 20>;
    using O3 = Order<103, BUY, 50, 30>;
    
    printType<O1>("Order 1 Type");
    printType<O2>("Order 2 Type");

    std::cout << "\n--- Creating an Empty Level at $50 ---\n";
    using StartingLevel = EmptyLevel<50>;
    printType<StartingLevel>("Initial Level Layout");

    std::cout << "\n--- Simulating Time Priority: Adding 3 Orders ---\n";
    using LevelL1 = AddOrderToLevel_t<StartingLevel, O1>;
    using LevelL2 = AddOrderToLevel_t<LevelL1, O2>;
    using CompleteLevel = AddOrderToLevel_t<LevelL2, O3>;
    printType<CompleteLevel>("Level after adding O1, O2, O3");

    std::cout << "\n--- Target Lock & Destroy: Erasing Order 102 ---\n";
    using CleanLevel = RemoveOrderFromLevel_t<102, CompleteLevel>;
    printType<CleanLevel>("Level after compile-time removal of Order 102");

    std::cout << "\n--- Hardware Check (0-byte RAM verification) ---\n";
    std::cout << "Size of CleanLevel in dynamic RAM: " << sizeof(CleanLevel) << " bytes\n";

    return 0;
}
