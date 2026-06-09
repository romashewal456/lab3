#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <algorithm>
#include <future>

// Скорочена копія логіки патернів та мультипоточності для ізольованого тестування
class SortStrategy {
public:
    virtual ~SortStrategy() = default;
    std::vector<int> sort(const std::vector<int>& arr) {
        std::vector<int> data = arr;
        doSort(data); 
        return data;
    }
    virtual void doSort(std::vector<int>& arr) = 0; 
};

class BubbleSort : public SortStrategy {
public:
    void doSort(std::vector<int>& arr) override {
        int n = arr.size();
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (arr[j] > arr[j + 1]) std::swap(arr[j], arr[j + 1]);
            }
        }
    }
};

class ParallelMergeSort : public SortStrategy {
private:
    void merge(std::vector<int>& arr, int l, int m, int r) {
        int n1 = m - l + 1, n2 = r - m;
        std::vector<int> L(n1), R(n2);
        for (int i = 0; i < n1; i++) L[i] = arr[l + i];
        for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];
        int i = 0, j = 0, k = l;
        while (i < n1 && j < n2) {
            if (L[i] <= R[j]) arr[k++] = L[i++];
            else arr[k++] = R[j++];
        }
        while (i < n1) arr[k++] = L[i++];
        while (j < n2) arr[k++] = R[j++];
    }
    void mergeSortParallel(std::vector<int>& arr, int l, int r, int depth) {
        if (l >= r) return;
        int m = l + (r - l) / 2;
        if (depth < 2) {
            auto handle = std::async(std::launch::async, &ParallelMergeSort::mergeSortParallel, this, std::ref(arr), l, m, depth + 1);
            mergeSortParallel(arr, m + 1, r, depth + 1);
            handle.wait();
        } else {
            mergeSortParallel(arr, l, m, depth + 1);
            mergeSortParallel(arr, m + 1, r, depth + 1);
        }
        merge(arr, l, m, r);
    }
public:
    void doSort(std::vector<int>& arr) override {
        if (arr.empty()) return;
        mergeSortParallel(arr, 0, arr.size() - 1, 0);
    }
};

// Самі тести
void testAlgorithms() {
    std::vector<int> data = {14, -3, 0, 99, 45, 11, -25, 8};
    std::vector<int> expected = data;
    std::sort(expected.begin(), expected.end()); // Еталонний результат

    BubbleSort bubble;
    ParallelMergeSort parallel;

    // Перевірка послідовного алгоритму
    assert(bubble.sort(data) == expected);
    std::cout << "[TEST PASSED]: Послідовний BubbleSort працює коректно!\n";

    // Перевірка мультипоточного паралельного алгоритму
    assert(parallel.sort(data) == expected);
    std::cout << "[TEST PASSED]: Мультипоточний ParallelMergeSort працює коректно!\n";
    
    // Порівняння результатів двох алгоритмів між собою
    assert(bubble.sort(data) == parallel.sort(data));
    std::cout << "[TEST PASSED]: Результати обох версій повністю ідентичні!\n";
}

int main() {
    std::cout << "===============================================\n";
    std::cout << "  ЗАПУСК ЮНІТ-ТЕСТІВ ДЛЯ ЛАБОРАТОРНОЇ РОБОТИ №3\n";
    std::cout << "===============================================\n";
    
    testAlgorithms();
    
    std::cout << "\n>>> Всі тести мультипоточності успішно пройдено! <<<\n";
    return 0;
}