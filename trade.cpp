
//  STOCK EXCHANGE MATCHING ENGINE — OPTIMIZED
//  Core order-book structures with try-catch error handling
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

//  CUSTOM EXCEPTION CLASSES 

class EngineException {
protected:
    std::string message;
public:
    EngineException(const std::string& msg) : message(msg) {}
    
    std::string what() const {
        return message;
    }
};

class InvalidOrderException : public EngineException {
public:
    InvalidOrderException(const std::string& msg) 
        : EngineException("Invalid Order: " + msg) {}
};

class OrderNotFoundException : public EngineException {
public:
    OrderNotFoundException(const std::string& msg) 
        : EngineException("Order Not Found: " + msg) {}
};

class PoolExhaustedException : public EngineException {
public:
    PoolExhaustedException(const std::string& msg) 
        : EngineException("Pool Exhausted: " + msg) {}
};

class FileOperationException : public EngineException {
public:
    FileOperationException(const std::string& msg) 
        : EngineException("File Operation Error: " + msg) {}
};

class InvalidCommandException : public EngineException {
public:
    InvalidCommandException(const std::string& msg) 
        : EngineException("Invalid Command: " + msg) {}
};

//  CONFIGURATION CONSTANTS

const int MaxOrders = 8192;
const int MaxLevels = 1024;
const int HashMapSize = 16384; //size to keep load factor low (< 50%) for Linear Probing
const int MaxTraderIdLength = 32;
const long long PriceScale = 10000LL;

//  SIDE CONSTANTS

const int BUY = 0;
const int SELL = 1;

const char* side_to_string(int side) {
    if (side == BUY) return "BUY";
    if (side == SELL) return "SELL";
    return "UNKNOWN";
}
//  ORDER STATUS CONSTANTS

const int PENDING = 0;
const int PARTIAL = 1;
const int COMPLETED = 2;
const int CANCELLED = 3;

const char* status_to_string(int status) {
    if (status == PENDING)   return "PENDING";
    if (status == PARTIAL)   return "PARTIAL";
    if (status == COMPLETED) return "COMPLETED";
    if (status == CANCELLED) return "CANCELLED";
    return "UNKNOWN";
}

//  UTILITY FUNCTIONS WITH EXCEPTION HANDLING

long long price_to_fixed(double value) {
    if (value < 0) {
        throw InvalidOrderException("Price cannot be negative");
    }
    return (long long)(value * PriceScale + 0.5);
}

double price_to_double(long long fixed_value) {
    if (fixed_value < 0) {
        throw InvalidOrderException("Fixed price value is negative");
    }
    return (double)fixed_value / PriceScale;
}

void validate_trader_id(const std::string& trader_id) {
    if (trader_id.empty()) {
        throw InvalidOrderException("Trader ID cannot be empty");
    }
    if (trader_id.length() > 100) {
        throw InvalidOrderException("Trader ID too long (max 100 characters)");
    }
}

void validate_quantity(int quantity) {
    if (quantity <= 0) {
        throw InvalidOrderException("Quantity must be greater than 0");
    }
    if (quantity > 1000000) {
        throw InvalidOrderException("Quantity exceeds maximum (1,000,000)");
    }
}

void validate_price(double price) {
    if (price <= 0) {
        throw InvalidOrderException("Price must be greater than 0");
    }
    if (price > 999999.99) {
        throw InvalidOrderException("Price exceeds maximum ($999,999.99)");
    }
}

void print_line(char c, int len) {
    for (int i = 0; i < len; i++) std::cout << c;
    std::cout << "\n";
}

int min_int(int a, int b) {
    return (a < b) ? a : b;
}

long long max_long(long long a, long long b) {
    return (a > b) ? a : b;
}

//  ORDER STRUCTURE

struct Order {
    int order_id;
    int side;
    long long price;
    int quantity;
    int remaining_qty;
    int status;
    std::string trader_id;
    
    Order* prev;
    Order* next;
    struct PriceLevel* parent_level;
    
    Order() 
        : order_id(0), side(BUY), price(0), quantity(0),
          remaining_qty(0), status(PENDING), trader_id(""),
          prev(0), next(0), parent_level(0)
    {
    }
};

//  PRICE LEVEL STRUCTURE

struct PriceLevel {
    long long price;
    Order* head;
    Order* tail;
    PriceLevel* prev;
    PriceLevel* next;
    
    PriceLevel()
        : price(0), head(0), tail(0), prev(0), next(0)
    {
    }
};

//  MEMORY POOL CLASS WITH EXCEPTION HANDLING

class OrderPool {
private:
    Order order_pool[MaxOrders];
    Order* free_list;
    int count;
    
public:
    OrderPool() : count(0) {
        try {
            reset();
        } catch (const EngineException& e) {
            std::cerr << "Error initializing OrderPool: " << e.what() << "\n";
            throw;
        }
    }
    
    void reset() {
        free_list = &order_pool[0];
        for (int i = 0; i < MaxOrders - 1; i++) {
            order_pool[i].next = &order_pool[i + 1];
        }
        order_pool[MaxOrders - 1].next = 0;
        count = 0;
    }
    
    Order* allocate() {
        if (!free_list) {
            throw PoolExhaustedException("No orders available in pool");
        }
        Order* ord = free_list;
        free_list = free_list->next;
        *ord = Order();
        count++;
        return ord;
    }
    
    void free(Order* ord) {
        if (!ord) {
            throw InvalidOrderException("Cannot free null order");
        }
        ord->next = free_list;
        free_list = ord;
        count--;
    }
    
    int get_count() const {
        return count;
    }
};

//  PRICE LEVEL POOL CLASS WITH EXCEPTION HANDLING

class PriceLevelPool {
private:
    PriceLevel level_pool[MaxLevels];
    PriceLevel* free_list;
    int count;
    
public:
    PriceLevelPool() : count(0) {
        try {
            reset();
        } catch (const EngineException& e) {
            std::cerr << "Error initializing PriceLevelPool: " << e.what() << "\n";
            throw;
        }
    }
    
    void reset() {
        free_list = &level_pool[0];
        for (int i = 0; i < MaxLevels - 1; i++) {
            level_pool[i].next = &level_pool[i + 1];
        }
        level_pool[MaxLevels - 1].next = 0;
        count = 0;
    }
    
    PriceLevel* allocate() {
        if (!free_list) {
            throw PoolExhaustedException("No price levels available in pool");
        }
        PriceLevel* lvl = free_list;
        free_list = free_list->next;
        *lvl = PriceLevel();
        count++;
        return lvl;
    }
    
    void free(PriceLevel* lvl) {
        if (!lvl) {
            throw InvalidOrderException("Cannot free null price level");
        }
        lvl->next = free_list;
        free_list = lvl;
        count--;
    }
};
// HASH MAP WITH OPEN ADDRESSING & LINEAR PROBING

class ActiveOrdersMap {
private:
    Order* hash_table[HashMapSize];
    bool is_deleted[HashMapSize]; // Tracking tombstone states for safe deletions
    int count;
    
    unsigned int hash_fn(int order_id) {
        return (unsigned int)order_id % HashMapSize;
    }
    
public:
    ActiveOrdersMap() : count(0) {
        clear();
    }
    
    void insert(Order* ord) {
        if (!ord) {
            throw InvalidOrderException("Cannot insert null order into map");
        }
        if (count >= HashMapSize / 2) { // Guard load factor < 50%
            throw PoolExhaustedException("ActiveOrdersMap limit exceeded (Load factor guard)");
        }
        
        int id = ord->order_id;
        unsigned int idx = hash_fn(id);
        unsigned int start_idx = idx;
        
        while (hash_table[idx] != 0) {
            if (hash_table[idx]->order_id == id) {
                hash_table[idx] = ord; // Update match if already exists
                return;
            }
            idx = (idx + 1) % HashMapSize;
            if (idx == start_idx) {
                throw PoolExhaustedException("ActiveOrdersMap is completely full");
            }
        }
        
        hash_table[idx] = ord;
        is_deleted[idx] = false;
        count++;
    }
    
    Order* find(int order_id) {
        unsigned int idx = hash_fn(order_id);
        unsigned int start_idx = idx;
        
        while (hash_table[idx] != 0 || is_deleted[idx]) {
            if (hash_table[idx] != 0 && hash_table[idx]->order_id == order_id) {
                return hash_table[idx];
            }
            idx = (idx + 1) % HashMapSize;
            if (idx == start_idx) break;
        }
        return 0;
    }
    
    void remove(int order_id) {
        unsigned int idx = hash_fn(order_id);
        unsigned int start_idx = idx;
        
        while (hash_table[idx] != 0 || is_deleted[idx]) {
            if (hash_table[idx] != 0 && hash_table[idx]->order_id == order_id) {
                hash_table[idx] = 0;
                is_deleted[idx] = true; // Use tombstone marker
                count--;
                return;
            }
            idx = (idx + 1) % HashMapSize;
            if (idx == start_idx) break;
        }
    }
    
    int count_active() const {
        return count;
    }
    
    void clear() {
        for (int i = 0; i < HashMapSize; i++) {
            hash_table[i] = 0;
            is_deleted[i] = false;
        }
        count = 0;
    }
};
//  MATCHING ENGINE WITH EXCEPTION HANDLING
class MatchingEngine {
private:
    OrderPool order_pool;
    PriceLevelPool level_pool;
    ActiveOrdersMap active_map;
    PriceLevel* buy_book_head;
    PriceLevel* sell_book_head;
    
    //Track active pointers in an sorted array for binary searching O(log M) 
    PriceLevel* active_buy_levels[MaxLevels];
    int buy_levels_count;
    PriceLevel* active_sell_levels[MaxLevels];
    int sell_levels_count;
    
    int next_order_id;
    long long highest_price;
    int total_volume;
    int total_orders;
    int completed_trades;

    // Helper method to keep index arrays synced up
    void rebuild_level_indexes() {
        buy_levels_count = 0;
        PriceLevel* curr = buy_book_head;
        while (curr && buy_levels_count < MaxLevels) {
            active_buy_levels[buy_levels_count++] = curr;
            curr = curr->next;
        }
        
        sell_levels_count = 0;
        curr = sell_book_head;
        while (curr && sell_levels_count < MaxLevels) {
            active_sell_levels[sell_levels_count++] = curr;
            curr = curr->next;
        }
    }

public:
    MatchingEngine()
        : buy_book_head(0), sell_book_head(0), buy_levels_count(0), sell_levels_count(0),
          next_order_id(1), highest_price(0), total_volume(0),
          total_orders(0), completed_trades(0)
    {
        try {
            order_pool.reset();
            level_pool.reset();
            active_map.clear();
        } catch (const EngineException& e) {
            std::cerr << "Error initializing MatchingEngine: " << e.what() << "\n";
            throw;
        }
    }
   
    // Place a new order with exception handling
   
    void place_order(int side, double price_double, int quantity,
                     const std::string& trader_id_str) {
        try {
            validate_price(price_double);
            validate_quantity(quantity);
            validate_trader_id(trader_id_str);
            
            if (side != BUY && side != SELL) {
                throw InvalidOrderException("Side must be BUY (0) or SELL (1)");
            }
            
            long long price = price_to_fixed(price_double);
            int new_order_id = next_order_id++;
            
            Order* new_order = order_pool.allocate();
            if (!new_order) {
                throw PoolExhaustedException("Failed to allocate order");
            }
            
            new_order->order_id = new_order_id;
            new_order->side = side;
            new_order->price = price;
            new_order->quantity = quantity;
            new_order->remaining_qty = quantity;
            new_order->status = PENDING;
            if (trader_id_str.length() > MaxTraderIdLength - 1) {
                new_order->trader_id = trader_id_str.substr(0, MaxTraderIdLength - 1);
            } else {
                new_order->trader_id = trader_id_str;
            }
            
            active_map.insert(new_order);
            total_orders++;
            
            match_order(new_order);
            
            if (new_order->remaining_qty == 0) {
                active_map.remove(new_order_id);
                order_pool.free(new_order);
            }
            
            std::cout << "  Order " << new_order_id << " placed ("
                      << side_to_string(side) << " "
                      << quantity << " @ $" << price_double << ")\n";
        } catch (const InvalidOrderException& e) {
            std::cerr << "  error: " << e.what() << "\n";
        } catch (const PoolExhaustedException& e) {
            std::cerr << "  error: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in place_order\n";
        }
    }
    
    // Core matching function
    
    void match_order(Order* incoming) {
        try {
            if (!incoming) {
                throw InvalidOrderException("Cannot match null order");
            }
            
            PriceLevel*& book_head = (incoming->side == BUY) ? sell_book_head : buy_book_head;
            PriceLevel*& add_book = (incoming->side == BUY) ? buy_book_head : sell_book_head;
            
            while (incoming->remaining_qty > 0 && book_head) {
                long long incoming_price = incoming->price;
                long long book_price = book_head->price;
                
                bool price_match = (incoming->side == BUY) 
                    ? (incoming_price >= book_price) 
                    : (incoming_price <= book_price);
                if (!price_match) break;
                
                PriceLevel* lvl = book_head;
                while (incoming->remaining_qty > 0 && lvl) {
                    Order* existing = lvl->head;
                    while (existing && incoming->remaining_qty > 0) {
                        int fill_qty = min_int(incoming->remaining_qty, existing->remaining_qty);
                        long long trade_price = existing->price;
                        
                        incoming->remaining_qty -= fill_qty;
                        existing->remaining_qty -= fill_qty;
                        incoming->status = (incoming->remaining_qty == 0) ? COMPLETED : PARTIAL;
                        existing->status = (existing->remaining_qty == 0) ? COMPLETED : PARTIAL;
                        
                        highest_price = max_long(highest_price, trade_price);
                        total_volume += fill_qty;
                        completed_trades++;
                        std::cout << "  >> TRADE: " << fill_qty << " shares @ $"
                                  << price_to_double(trade_price)
                                  << " (buyer: " << (incoming->side == BUY ? incoming->trader_id : existing->trader_id)
                                  << ", seller: " << (incoming->side == SELL ? incoming->trader_id : existing->trader_id) << ")\n";
                        if (existing->remaining_qty == 0) {
                            Order* next_order = existing->next;
                            active_map.remove(existing->order_id);
                            remove_from_level(lvl, existing);
                            order_pool.free(existing);
                            existing = next_order;
                        } else {
                            existing = existing->next;
                        }
                    }
                    
                    if (lvl->head == 0) {
                        PriceLevel* next_lvl = lvl->next;
                        if (lvl->prev) {
                            lvl->prev->next = next_lvl;
                        } else {
                            book_head = next_lvl;
                        }
                        if (next_lvl) {
                            next_lvl->prev = lvl->prev;
                        }
                        level_pool.free(lvl);
                        lvl = next_lvl;
                        rebuild_level_indexes();
                    } else {
                        lvl = lvl->next;
                    }
                }
                
                book_head = lvl;
            }
            
            if (incoming->remaining_qty > 0) {
                insert_into_book(incoming, add_book);
            }
        } catch (const EngineException& e) {
            std::cerr << "  error in match_order: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in match_order\n";
        }
    }
    
    // OPTIMIZED: Insert order into order book via O(log M) Binary Search

    void insert_into_book(Order* ord, PriceLevel*& book_head) {
        try {
            if (!ord) {
                throw InvalidOrderException("Cannot insert null order into book");
            }
            
            long long price = ord->price;
            PriceLevel** levels_arr = (ord->side == BUY) ? active_buy_levels : active_sell_levels;
            int levels_count = (ord->side == BUY) ? buy_levels_count : sell_levels_count;
            
            int low = 0;
            int high = levels_count - 1;
            int insert_pos = -1;
            PriceLevel* lvl = 0;
            
            // Binary search to locate level or placement insertion position
            while (low <= high) {
                int mid = low + (high - low) / 2;
                if (levels_arr[mid]->price == price) {
                    lvl = levels_arr[mid];
                    break;
                }
                
                bool standard_priority = (ord->side == BUY) ? (price < levels_arr[mid]->price) : (price > levels_arr[mid]->price);
                if (standard_priority) {
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }
            
            if (lvl) {
                append_to_level(lvl, ord);
            } else {
                PriceLevel* new_level = level_pool.allocate();
                if (!new_level) {
                    throw PoolExhaustedException("Failed to allocate price level");
                }
                
                new_level->price = price;
                append_to_level(new_level, ord);
                
                // Linear scan replacement: find direct link point using low bounds
                PriceLevel* curr = book_head;
                PriceLevel* prev = 0;
                while (curr) {
                    bool structural_cond = (ord->side == BUY) ? (price < curr->price) : (price > curr->price);
                    if (structural_cond) {
                        prev = curr;
                        curr = curr->next;
                    } else {
                        break;
                    }
                }
                
                if (!prev) {
                    new_level->prev = 0;
                    new_level->next = book_head;
                    if (book_head) {
                        book_head->prev = new_level;
                    }
                    book_head = new_level;
                } else {
                    new_level->next = curr;
                    new_level->prev = prev;
                    if (curr) {
                        curr->prev = new_level;
                    }
                    prev->next = new_level;
                }
                rebuild_level_indexes();
            }
        } catch (const EngineException& e) {
            std::cerr << "  ERROR in insert_into_book: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  UNEXPECTED ERROR in insert_into_book\n";
        }
    }

    // Append order to a price level

    void append_to_level(PriceLevel* lvl, Order* ord) {
        try {
            if (!lvl || !ord) {
                throw InvalidOrderException("Cannot append: null level or order");
            }
            
            ord->prev = lvl->tail;
            ord->next = 0;
            ord->parent_level = lvl;
            
            if (lvl->tail) {
                lvl->tail->next = ord;
            } else {
                lvl->head = ord;
            }
            lvl->tail = ord;
        } catch (const EngineException& e) {
            std::cerr << "  error in append_to_level: " << e.what() << "\n";
        }
    }
    
    // Remove order from a price level

    void remove_from_level(PriceLevel* lvl, Order* ord) {
        try {
            if (!lvl || !ord) {
                throw InvalidOrderException("Cannot remove: null level or order");
            }
            
            if (ord->prev) {
                ord->prev->next = ord->next;
            } else {
                lvl->head = ord->next;
            }
            
            if (ord->next) {
                ord->next->prev = ord->prev;
            } else {
                lvl->tail = ord->prev;
            }
            
            ord->prev = 0;
            ord->next = 0;
            ord->parent_level = 0;
        } catch (const EngineException& e) {
            std::cerr << "  Error in remove_from_level: " << e.what() << "\n";
        }
    }
   
    // Cancel an order with exception handling
    void cancel_order(int order_id) {
        try {
            Order* ord = active_map.find(order_id);
            if (!ord) {
                throw OrderNotFoundException("Order ID " + int_to_string(order_id));
            }
            
            PriceLevel* lvl = ord->parent_level;
            if (!lvl) {
                throw OrderNotFoundException("Order has no price level");
            }
            
            ord->status = CANCELLED;
            remove_from_level(lvl, ord);
            active_map.remove(order_id);
            order_pool.free(ord);
            
            if (lvl->head == 0) {
                PriceLevel*& book_head = (ord->side == BUY) ? buy_book_head : sell_book_head;
                if (lvl->prev) {
                    lvl->prev->next = lvl->next;
                } else {
                    book_head = lvl->next;
                }
                if (lvl->next) {
                    lvl->next->prev = lvl->prev;
                }
                level_pool.free(lvl);
                rebuild_level_indexes();
            }
            
            std::cout << "  Order " << order_id << " cancelled\n";
        } catch (const OrderNotFoundException& e) {
            std::cerr << "  ERROR: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in cancel_order\n";
        }
    }
    
    // Modify an order with exception handling
    void modify_order(int order_id, double new_price, int new_qty) {
        try {
            Order* ord = active_map.find(order_id);
            if (!ord) {
                throw OrderNotFoundException("Order ID " + int_to_string(order_id));
            }
            
            validate_price(new_price);
            validate_quantity(new_qty);
            
            std::string trader = ord->trader_id;
            int side = ord->side;
            cancel_order(order_id);
            place_order(side, new_price, new_qty, trader);
        } catch (const OrderNotFoundException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const InvalidOrderException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in modify_order\n";
        }
    }
    
    // Display a single order with exception handling
   
    void display_order(int order_id) {
        try {
            Order* ord = active_map.find(order_id);
            if (!ord) {
                throw OrderNotFoundException("Order ID " + int_to_string(order_id));
            }
            
            print_line('-', 55);
            std::cout << "  Order ID        : " << ord->order_id << "\n"
                      << "  Side            : " << side_to_string(ord->side) << "\n"
                      << "  Price           : $" << price_to_double(ord->price) << "\n"
                      << "  Original Qty    : " << ord->quantity << "\n"
                      << "  Remaining Qty   : " << ord->remaining_qty << "\n"
                      << "  Status          : " << status_to_string(ord->status) << "\n"
                      << "  Trader ID       : " << ord->trader_id << "\n";
            print_line('-', 55);
        } catch (const OrderNotFoundException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in display_order\n";
        }
    }
    // Display order books
    void display_order_books() {
        try {
            print_line('=', 55);
            std::cout << "  Buy book (descending price)\n";
            print_line('-', 55);
            if (!buy_book_head) {
                std::cout << "  (empty)\n";
            } else {
                PriceLevel* lvl = buy_book_head;
                while (lvl) {
                    Order* ord = lvl->head;
                    while (ord) {
                        std::cout << "    Price: $" << price_to_double(ord->price)
                                  << "  Qty: " << ord->remaining_qty
                                  << "  Trader: " << ord->trader_id << "\n";
                        ord = ord->next;
                    }
                    lvl = lvl->next;
                }
            }
            
            print_line('=', 55);
            std::cout << "  sell book (ascending price)\n";
            print_line('-', 55);
            if (!sell_book_head) {
                std::cout << "  (empty)\n";
            } else {
                PriceLevel* lvl = sell_book_head;
                while (lvl) {
                    Order* ord = lvl->head;
                    while (ord) {
                        std::cout << "    Price: $" << price_to_double(ord->price)
                                  << "  Qty: " << ord->remaining_qty
                                  << "  Trader: " << ord->trader_id << "\n";
                        ord = ord->next;
                    }
                    lvl = lvl->next;
                }
            }
            
            print_line('=', 55);
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in display_order_books\n";
        }
    }
    // Display statistics with exception handling

    void display_statistics() {
        try {
            print_line('=', 55);
            std::cout << "  ENGINE STATISTICS\n";
            print_line('-', 55);
            
            double lp = (highest_price > 0) ? price_to_double(highest_price) : 0.0;
            double avg = (completed_trades > 0) ? ((double)total_volume / completed_trades) : 0.0;
            std::cout << "  Total orders placed   : " << total_orders << "\n"
                      << "  Completed trades      : " << completed_trades << "\n"
                      << "  Active orders         : " << active_map.count_active() << "\n"
                      << "  Total volume (shares) : " << total_volume << "\n"
                      << "  Highest trade price   : $" << price_to_double(highest_price) << "\n"
                      << "  Lowest trade price    : $" << lp << "\n"
                      << "  Average trade price   : $" << avg << "\n";
            print_line('=', 55);
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in display_statistics\n";
        }
    }
   
    // Save to file with exception handling
    void save_to_file(const std::string& filename) {
        try {
            if (filename.empty()) {
                throw InvalidCommandException("Filename cannot be empty");
            }
            
            std::ofstream file(filename.c_str());
            if (!file.is_open()) {
                throw FileOperationException("Cannot open file: " + filename);
            }
            
            PriceLevel* lvl = buy_book_head;
            while (lvl) {
                Order* ord = lvl->head;
                while (ord) {
                    file << "BUY "
                         << price_to_double(ord->price) << " "
                         << ord->remaining_qty << " "
                         << ord->trader_id << "\n";
                    if (!file.good()) {
                        throw FileOperationException("Write error to file");
                    }
                    ord = ord->next;
                }
                lvl = lvl->next;
            }
            
            lvl = sell_book_head;
            while (lvl) {
                Order* ord = lvl->head;
                while (ord) {
                    file << "SELL "
                         << price_to_double(ord->price) << " "
                         << ord->remaining_qty << " "
                         << ord->trader_id << "\n";
                    if (!file.good()) {
                        throw FileOperationException("Write error to file");
                    }
                    ord = ord->next;
                }
                lvl = lvl->next;
            }
            
            file.close();
            std::cout << "  Active orders saved to " << filename << "\n";
        } catch (const FileOperationException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const InvalidCommandException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in save_to_file\n";
        }
    }
    // Load from file with exception handling

    void load_orders_from_file(const std::string& filename) {
        try {
            if (filename.empty()) {
                throw InvalidCommandException("Filename cannot be empty");
            }
            
            std::ifstream file(filename.c_str());
            if (!file.is_open()) {
                throw FileOperationException("Cannot open file: " + filename);
            }
            
            std::cout << "  Loading orders from " << filename << "...\n";
            print_line('-', 55);
            
            std::string line;
            int count = 0;
            int line_num = 0;
            while (std::getline(file, line)) {
                line_num++;
                if (line.empty() || line[0] == '#') continue;
                
                try {
                    if (process_command(line)) {
                        count++;
                    }
                } catch (const EngineException& e) {
                    std::cerr << "  Line " << line_num << ": " << e.what() << "\n";
                }
            }
            
            file.close();
            print_line('-', 55);
            std::cout << "  Processed " << count << " commands from " << filename << "\n";
        } catch (const FileOperationException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const InvalidCommandException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "  Unexpected error in load_orders_from_file\n";
        }
    }

    // Process command with exception handling
    bool process_command(const std::string& line) {
        try {
            if (line.empty()) return true;
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            
            if (!iss.good() && !iss.eof()) {
                throw InvalidCommandException("Failed to process command");
            }
            
            for (size_t i = 0; i < cmd.size(); i++) {
                if (cmd[i] >= 'a' && cmd[i] <= 'z') {
                    cmd[i] -= 32;
                }
            }
            
            if (cmd == "DONE" || cmd == "EXIT" || cmd == "QUIT") {
                return false;
            }
            
            if (cmd == "BUY" || cmd == "SELL") {
                double price;
                int qty;
                std::string trader;
                
                if (!(iss >> price >> qty >> trader)) {
                    throw InvalidCommandException(
                        "Usage: " + cmd + " <price> <quantity> <trader_id>");
                }
                
                int side = (cmd == "BUY") ? BUY : SELL;
                place_order(side, price, qty, trader);
            }
            else if (cmd == "CANCEL") {
                int id;
                if (!(iss >> id)) {
                    throw InvalidCommandException("Usage: CANCEL <order_id>");
                }
                cancel_order(id);
            }
            else if (cmd == "MODIFY") {
                int id;
                double new_price;
                int new_qty;
                
                if (!(iss >> id >> new_price >> new_qty)) {
                    throw InvalidCommandException(
                        "Usage: MODIFY <order_id> <new_price> <new_qty>");
                }
                modify_order(id, new_price, new_qty);
            }
            else if (cmd == "VIEW") {
                int id;
                if (!(iss >> id)) {
                    throw InvalidCommandException("Usage: VIEW <order_id>");
                }
                display_order(id);
            }
            else if (cmd == "BOOK") {
                display_order_books();
            }
            else if (cmd == "STATS") {
                display_statistics();
            }
            else if (cmd == "SAVE") {
                std::string filename;
                if (!(iss >> filename)) {
                    throw InvalidCommandException("Usage: SAVE <filename>");
                }
                save_to_file(filename);
            }
            else if (cmd == "LOAD") {
                std::string filename;
                if (!(iss >> filename)) {
                    throw InvalidCommandException("Usage: LOAD <filename>");
                }
                load_orders_from_file(filename);
            }
            else if (cmd == "HELP") {
                print_line('=', 55);
                std::cout << "  AVAILABLE COMMANDS\n";
                print_line('-', 55);
                std::cout
                    << "  BUY   <price> <qty> <trader>    Place buy order\n"
                    << "  SELL  <price> <qty> <trader>    Place sell order\n"
                    << "  CANCEL <order_id>               Cancel order\n"
                    << "  MODIFY <id> <price> <qty>       Modify order\n"
                    << "  VIEW   <order_id>               View order details\n"
                    << "  BOOK                            View order books\n"
                    << "  STATS                           View statistics\n"
                    << "  SAVE   <filename>               Save active orders\n"
                    << "  LOAD   <filename>               Load from file\n"
                    << "  HELP                            Show this help\n"
                    << "  DONE                            Exit engine\n";
                print_line('=', 55);
            }
            else {
                throw InvalidCommandException("Unknown command: " + cmd);
            }
            
            return true;
        } catch (const InvalidCommandException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
            return true;
        } catch (const EngineException& e) {
            std::cerr << "  Error: " << e.what() << "\n";
            return true;
        } catch (...) {
            std::cerr << "  Unexpected error in process_command\n";
            return true;
        }
    }
    
    std::string int_to_string(int value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
};
//  MAIN FUNCTION WITH EXCEPTION HANDLING

int main(int argc, char* argv[]) {
    try {
        MatchingEngine engine;
        print_line('=', 55);
        std::cout << "  Stock Exchange Matching Engine\n"
                  << "  Price-Time Priority | With Optimized Data Layouts\n"
                  << "  Type HELP for available commands\n";
        print_line('=', 55);
        
        // Batch mode
        if (argc > 1) {
            for (int i = 1; i < argc; i++) {
                try {
                    engine.load_orders_from_file(argv[i]);
                } catch (const EngineException& e) {
                    std::cerr << "Error processing file " << argv[i] << ": "
                              << e.what() << "\n";
                }
            }
            std::cout << "\n  Batch processing complete.\n";
            engine.display_order_books();
            engine.display_statistics();
        }
        
        // Interactive mode
        std::string line;
        while (true) {
            try {
                std::cout << "\n  > ";
                if (!std::getline(std::cin, line)) {
                    std::cout << "\n";
                    break;
                }
                if (!engine.process_command(line)) break;
            } catch (const EngineException& e) {
                std::cerr << "  Error: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "  Unexpected error in main loop\n";
            }
        }
        
        std::cout << "\n  Final state:\n";
        engine.display_statistics();
        std::cout << "\n  Engine shutdown gracefully.\n";
        
        return 0;
    } catch (const EngineException& e) {
        std::cerr << "  Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "  Unknown fatal error \n";
        return 1;
    }
}