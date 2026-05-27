#include <iostream>
#include <type_traits>
#include <typeinfo>
#include <cstdlib>

// Cross-platform demangler for Apple Silicon M3 (Clang) and GCC
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
template <typename T>
void printType(const char* label) {
    int status;
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
// 1. THE DATA STRUCTURES & MERGE UTILITY
// ============================================================================

template <int PriceValue>
struct PriceLevel {
    static constexpr int price = PriceValue;
};

template <typename... Args>
struct List {};

template <typename L1, typename L2>
struct ListMerge;

template <typename... Elem1, typename... Elem2>
struct ListMerge<List<Elem1...>, List<Elem2...>> {
    using type = List<Elem1..., Elem2...>;
};

template <typename L1, typename L2>
using ListMerge_t = typename ListMerge<L1, L2>::type;


// ============================================================================
// 2. THE CORRECTED NON-AMBIGUOUS SUBLIST SLICER
// ============================================================================
template<typename L, int Offset, int Size>
struct SubList;

// RULE 1: MASTER BASE CASE - If Size is 0, ALWAYS return an empty list immediately.
// This takes absolute priority over everything else.
template<typename... L, int Offset>
struct SubList<List<L...>, Offset, 0> {
    using type = List<>;
};

// RULE 2: If the list itself is empty, return an empty list.
template<int Offset, int Size>
struct SubList<List<>, Offset, Size> {
    using type = List<>;
};

// Special override case to prevent ambiguity when list is empty AND size is 0
template<int Offset>
struct SubList<List<>, Offset, 0> {
    using type = List<>;
};

// RULE 3: When Offset is exactly 0 and Size > 0, we are COLLECTING elements.
template<typename L1, typename... L2toN, int Size>
struct SubList<List<L1, L2toN...>, 0, Size> {
    using type = ListMerge_t<List<L1>, typename SubList<List<L2toN...>, 0, Size - 1>::type>;
};

// RULE 4: When Offset > 0 and Size > 0, we are SKIPPING elements.
template<typename L1, typename... L2toN, int Offset, int Size>
struct SubList<List<L1, L2toN...>, Offset, Size> {
    using type = typename SubList<List<L2toN...>, Offset - 1, Size>::type;
};

template<typename L, int Offset, int Size>
using SubList_t = typename SubList<L, Offset, Size>::type;


// ============================================================================
// 3. THE CONDITIONAL ZIPPER ENGINE (THE MERGE)
// ============================================================================
template <typename List1, typename List2, bool Ascending>
struct Merge;

// Base Case A: Left list runs out of elements -> dump remaining Right elements
template <typename... Ys, bool Ascending>
struct Merge<List<>, List<Ys...>, Ascending> {
    using type = List<Ys...>;
};

// Base Case B: Right list runs out of elements -> dump remaining Left elements
template <typename... Xs, bool Ascending>
struct Merge<List<Xs...>, List<>, Ascending> {
    using type = List<Xs...>;
};

// Base Case C: Both lists are empty
template <bool Ascending>
struct Merge<List<>, List<>, Ascending> {
    using type = List<>;
};

// Core Merge Machine: Compares front elements (X1 vs Y1) and routes selection
template<typename X1, typename... X2toN,
         typename Y1, typename... Y2toN,
         bool Ascending>
struct Merge<List<X1, X2toN...>, List<Y1, Y2toN...>, Ascending> {
    using type = std::conditional_t<
        Ascending,
        // --- Ascending Logic (Low to High) ---
        std::conditional_t<
            (X1::price <= Y1::price),
            ListMerge_t<List<X1>, typename Merge<List<X2toN...>, List<Y1, Y2toN...>, Ascending>::type>, // Left Wins
            ListMerge_t<List<Y1>, typename Merge<List<X1, X2toN...>, List<Y2toN...>, Ascending>::type>  // Right Wins
        >,
        // --- Descending Logic (High to Low) ---
        std::conditional_t<
            (X1::price >= Y1::price),
            ListMerge_t<List<X1>, typename Merge<List<X2toN...>, List<Y1, Y2toN...>, Ascending>::type>, // Left Wins
            ListMerge_t<List<Y1>, typename Merge<List<X1, X2toN...>, List<Y2toN...>, Ascending>::type>  // Right Wins
        >
    >;
};

template<typename List1, typename List2, bool Ascending>
using Merge_t = typename Merge<List1, List2, Ascending>::type;


// ============================================================================
// 4. THE MASTER MERGESORT ORCHESTRATOR
// ============================================================================
template <typename List, bool Ascending>
struct MergeSort;

// Base Case: A single element is already sorted!
template <typename T, bool Ascending>
struct MergeSort<List<T>, Ascending> {
    using type = List<T>;
};

// Base Case: Zero elements is already sorted!
template <bool Ascending>
struct MergeSort<List<>, Ascending> {
    using type = List<>;
};

// Recursive Case: Counts the parameter pack, breaks it in half, and triggers zipping
template <typename... Elements, bool Ascending>
struct MergeSort<List<Elements...>, Ascending> {
    using CurrentList = List<Elements...>;

    static constexpr int TotalSize = sizeof...(Elements);
    static constexpr int MidPoint  = TotalSize / 2;

    // Execute Slices using our safe non-overlapping SubList specializations
    using LeftHalf  = SubList_t<CurrentList, 0, MidPoint>;
    using RightHalf = SubList_t<CurrentList, MidPoint, TotalSize - MidPoint>;

    // Deep Recursion: Drill down both halves to atomic states
    using SortedLeft  = typename MergeSort<LeftHalf, Ascending>::type;
    using SortedRight = typename MergeSort<RightHalf, Ascending>::type;

    // Up Step: Rebuild the layers via the conditional zipper engine
    using type = Merge_t<SortedLeft, SortedRight, Ascending>;
};

template <typename List, bool Ascending>
using MergeSort_t = typename MergeSort<List, Ascending>::type;


// ============================================================================
// 5. RUNNABLE SIMULATION
// ============================================================================
int main() {
    using UnsortedInput = List<
        PriceLevel<2>, 
        PriceLevel<1>, 
        PriceLevel<3>, 
        PriceLevel<4>, 
        PriceLevel<6>, 
        PriceLevel<5>
    >;

    std::cout << "========================================================\n";
    std::cout << "COMPILE-TIME MERGE SORT ENGINE\n";
    std::cout << "========================================================\n";
    printType<UnsortedInput>("Raw Input Array State    ");

    using SortedLowToHigh = MergeSort_t<UnsortedInput, true>;
    printType<SortedLowToHigh>("\nSorted Result (Ascending) ");

    using SortedHighToLow = MergeSort_t<UnsortedInput, false>;
    printType<SortedHighToLow>("Sorted Result (Descending)");
    
    std::cout << "========================================================\n";
    return 0;
}
