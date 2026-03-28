#include <iostream>
#include <vector>

class HashMap {
private:
    struct Entry {
        int x, y;
        int strength;
        bool occupied;
        
        Entry() : x(0), y(0), strength(-1), occupied(false) {}
    };
    
    std::vector<Entry> table;
    int capacity;
    int size;
    
    int hash(int x, int y) const {
        return (x * 73856093 ^ y * 19349663) & (capacity - 1);
    }
    
    void rehash(int new_capacity) {
        std::vector<Entry> old_table = move(table);
        capacity = new_capacity;
        table.resize(capacity);
        size = 0;
        
        for (const auto& entry : old_table) {
            if (entry.occupied) {
                set(entry.x, entry.y, entry.strength);
            }
        }
    }
    
public:
    HashMap(int initial_cap = 1024) : capacity(initial_cap), size(0) {
        table.resize(capacity);
    }
    
    void set(int x, int y, int strength) {
        if (size * 10 > capacity * 7) {
            rehash(capacity * 2);
        }
        
        int h = hash(x, y);
        while (table[h].occupied) {
            if (table[h].x == x && table[h].y == y) {
                table[h].strength = strength;
                return;
            }
            h = (h + 1) & (capacity - 1);
        }
        
        table[h].x = x;
        table[h].y = y;
        table[h].strength = strength;
        table[h].occupied = true;
        size++;
    }
    
    int get(int x, int y) const {
        int h = hash(x, y);
        int start = h;
        
        while (table[h].occupied) {
            if (table[h].x == x && table[h].y == y) {
                return table[h].strength;
            }
            h = (h + 1) & (capacity - 1);
            if (h == start) break;
        }
        return -1;
    }
};

struct QueueItem {
    int x, y;
    int priority;
};

class PriorityQueue {
private:
    std::vector<QueueItem> heap;
    
    void sift_up(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (heap[parent].priority <= heap[i].priority) break;
            std::swap(heap[parent], heap[i]);
            i = parent;
        }
    }
    
    void sift_down(int i) {
        while (true) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            
            if (left < (int)heap.size() && heap[left].priority < heap[smallest].priority)
                smallest = left;
            if (right < (int)heap.size() && heap[right].priority < heap[smallest].priority)
                smallest = right;
            if (smallest == i) break;
            
            std::swap(heap[i], heap[smallest]);
            i = smallest;
        }
    }
    
public:
    void push(const QueueItem& item) {
        heap.push_back(item);
        sift_up(heap.size() - 1);
    }
    
    QueueItem pop() {
        QueueItem result = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) sift_down(0);
        return result;
    }
    
    bool empty() const {
        return heap.empty();
    }
};

int target_x, target_y, N;
int total_spent = 0;
HashMap cell_map;

const int dx[] = {0, 0, 1, -1};
const int dy[] = {1, -1, 0, 0};

int manhattan(int x, int y) {
    return abs(x - target_x) + abs(y - target_y);
}

int calc_priority(int strength, int x, int y) {
    int dist = manhattan(x, y);
    return strength + 10.5*dist;
}

int main() {
    std::cin >> target_x >> target_y >> N;
    
    cell_map.set(0, 0, 0);
    
    PriorityQueue pq;
    
    for (int i = 0; i < 4; i++) {
        int nx = dx[i], ny = dy[i], str;
        std::cin >> str;
        
        cell_map.set(nx, ny, str);
        
        if (str > 0 && total_spent + str <= N) {
            int priority = calc_priority(str, nx, ny);
            pq.push({nx, ny, priority});
        }
    }
    
    while (!pq.empty()) {
        QueueItem cur = pq.pop();
        
        if (cell_map.get(cur.x, cur.y) == 0) continue;
        
        int strength = cell_map.get(cur.x, cur.y);
        if (total_spent + strength > N) continue;
        
        if (cur.x == target_x && cur.y == target_y) {
            std::cout << cur.x << " " << cur.y << std::endl;
            return 0;
        }
        
        std::cout << cur.x << " " << cur.y << std::endl;
        total_spent += strength;
        
        cell_map.set(cur.x, cur.y, 0);
        
        for (int i = 0; i < 4; i++) {
            int nx = cur.x + dx[i];
            int ny = cur.y + dy[i];
            int str;
            std::cin >> str;
            
            if (cell_map.get(nx, ny) == -1) {
                cell_map.set(nx, ny, str);
            }
            
            if (str > 0 && total_spent + str <= N) {
                int priority = calc_priority(str, nx, ny);
                pq.push({nx, ny, priority});
            }
        }
    }
    
    return 0;
}