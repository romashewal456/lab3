#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <future> // ДЛЯ МУЛЬТИПОТОЧНОСТІ (std::async)
#include "httplib.h" 

// ==========================================
// БАЗОВИЙ ІНТЕРФЕЙС СТРАТЕГІЇ (Шаблонний метод залишається!)
// ==========================================
class SortStrategy {
public:
    virtual ~SortStrategy() = default;

    std::pair<std::vector<int>, long long> sort(const std::vector<int>& arr) {
        std::vector<int> data = arr;
        
        auto start = std::chrono::high_resolution_clock::now();
        doSort(data); 
        auto end = std::chrono::high_resolution_clock::now();
        
        long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return {data, duration};
    }

// Змінив на public, щоб Декоратор у тестах і в коді не сварився
    virtual void doSort(std::vector<int>& arr) = 0; 
};

// --- СТАРА СТРАТЕГІЯ З ЛАБИ 2 (Послідовна) ---
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

// --- НОВА МУЛЬТИПОТОЧНА СТРАТЕГІЯ ДЛЯ ЛАБИ 3 (Паралельна) ---
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

        // РОЗПАРАЛЕЛЬОВУВАННЯ: Якщо глибина рекурсії невелика, запускаємо ліву частину в окремому потоці CPU!
        if (depth < 2) {
            auto handle = std::async(std::launch::async, &ParallelMergeSort::mergeSortParallel, this, std::ref(arr), l, m, depth + 1);
            mergeSortParallel(arr, m + 1, r, depth + 1);
            handle.wait(); // Чекаємо завершення потоку
        } else {
            // Звичайне послідовне сортування (1 потік), щоб не перевантажувати CPU
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

// ==========================================
// ДЕКОРАТОР (Логування роботи)
// ==========================================
class ProfilerDecorator : public SortStrategy {
private:
    std::shared_ptr<SortStrategy> wrappedStrategy;
public:
    ProfilerDecorator(std::shared_ptr<SortStrategy> strategy) : wrappedStrategy(strategy) {}

    void doSort(std::vector<int>& arr) override {
        std::cout << "[MUTEX/THREAD LOG]: Запуск стратегії через Декоратор...\n";
        wrappedStrategy->doSort(arr);
        std::cout << "[MUTEX/THREAD LOG]: Потік завершив обробку даних.\n";
    }
};

// ==========================================
// РОЗШИРЕНА ФАБРИКА ПАТЕРНІВ
// ==========================================
class SortFactory {
public:
    static std::shared_ptr<SortStrategy> createStrategy(const std::string& type) {
        if (type == "bubble") {
            return std::make_shared<ProfilerDecorator>(std::make_shared<BubbleSort>());
        } else if (type == "parallel_merge") {
            // Фабрика тепер вміє створювати новий багатопоточний об'єкт!
            return std::make_shared<ProfilerDecorator>(std::make_shared<ParallelMergeSort>());
        }
        return nullptr;
    }
};

// Ітератор для JSON
std::string vectorToJson(std::vector<int>& arr) {
    std::stringstream ss;
    ss << "[";
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        ss << *it;
        if (it + 1 != arr.end()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

// ==========================================
// ВЕБ-СЕРВЕРЗ ДВОМА ЛАБАМИ
// ==========================================
int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("web/index.html"); 
        std::stringstream buffer;
        buffer << file.rdbuf();
        res.set_content(buffer.str(), "text/html; charset=utf-8");
    });

    svr.Get("/api/sort", [](const httplib::Request& req, httplib::Response& res) {
        std::string algo = req.has_param("algo") ? req.get_param_value("algo") : "bubble";
        
        // Генеруємо великий масив для Лаби 3, щоб було видно ефект швидкості
        std::vector<int> raw_data;
        for (int i = 100; i > 0; i--) raw_data.push_back(i * 7 % 100); // Хаотичні дані
        
        std::vector<int> original_copy = raw_data; 
        
        auto strategy = SortFactory::createStrategy(algo);
        if (!strategy) {
            res.set_content("{\"error\":\"Unknown algorithm\"}", "application/json");
            return;
        }

        auto result = strategy->sort(raw_data);
        std::vector<int> sorted_data = result.first;
        long long time_taken = result.second;

        std::stringstream json;
        json << "{\n"
             << "  \"original\": " << vectorToJson(original_copy) << ",\n"
             << "  \"sorted\": " << vectorToJson(sorted_data) << ",\n"
             << "  \"time\": " << time_taken << "\n"
             << "}";

        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    std::cout << "Сервер Спільних Лабораторних 2 і 3 запущено: http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}