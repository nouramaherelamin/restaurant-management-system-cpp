/*
 * Restaurant Management System - Win32 GUI edition
 * ECU - CSC-1200 Computer Programming II - Project 6
 *
 * Pure Win32 API (no external libraries).
 * Build:  see README.md  (g++ ... -mwindows -lcomctl32 -lgdi32)
 *
 * Layout of this file
 *   1. Utilities (strings, time, SHA-256)
 *   2. Domain model   : MenuItem, Order, Reservation
 *   3. Persistence    : Database (text files)
 *   4. Users          : User (abstract) -> Admin, Customer, Auth
 *   5. Win32 windows  : AppWindow (base) -> LoginWindow, AdminWindow, CustomerWindow
 *   6. WinMain
 */
#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// ============================================================================
//  Global GUI state / control IDs
// ============================================================================
static HINSTANCE g_hInst = nullptr;
static HFONT g_font = nullptr;
static HFONT g_fontBig = nullptr;
static const char* const WND_CLASS = "RMS_MainWindow";
static const char* const APP_TITLE = "Restaurant Management System";
static const UINT WM_APP_LOGOUT = WM_APP + 1;
static const int TABLE_COUNT = 10;
static const char* const SLOTS[] = {"12:00", "13:00", "14:00", "15:00", "16:00", "17:00",
                                    "18:00", "19:00", "20:00", "21:00", "22:00", "23:00"};

enum {
    ID_NAV = 100,  // 100..109 navigation buttons
    ID_LOGOUT = 110,

    ID_L_USER = 200, ID_L_PASS, ID_L_EMAIL, ID_L_LOGIN, ID_L_REGISTER, ID_L_EXIT,

    ID_AM_LIST = 300, ID_AM_SEARCH, ID_AM_NAME, ID_AM_CAT, ID_AM_PRICE, ID_AM_STOCK,
    ID_AM_ADD, ID_AM_UPDATE, ID_AM_DELETE, ID_AM_CLEAR,

    ID_AO_LIST = 330, ID_AO_FILTER, ID_AO_REFRESH, ID_AO_ACCEPT, ID_AO_REJECT, ID_AO_RECEIPT,

    ID_AR_LIST = 360, ID_AR_FILTER, ID_AR_REFRESH, ID_AR_CANCEL, ID_AR_FREE,

    ID_AS_DATE = 380, ID_AS_LIST, ID_AS_SUMMARY, ID_AS_SHOW,

    ID_CO_CAT = 400, ID_CO_SORT, ID_CO_SEARCH, ID_CO_MENU, ID_CO_QTY, ID_CO_ADD,
    ID_CO_CART, ID_CO_REMOVE, ID_CO_CLEAR, ID_CO_TOTAL, ID_CO_NAME, ID_CO_ADDR,
    ID_CO_PHONE, ID_CO_TYPE, ID_CO_TABLE, ID_CO_PLACE,

    ID_CR_DATE = 440, ID_CR_TIME, ID_CR_TABLES, ID_CR_RESERVE, ID_CR_MINE, ID_CR_CANCEL,

    ID_CH_LIST = 460, ID_CH_REFRESH, ID_CH_RECEIPT
};

// ============================================================================
//  1. Utilities
// ============================================================================
static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Remove characters that would break the comma / semicolon separated files.
static string clean(string s) {
    for (char& c : s)
        if (c == ',' || c == ';' || c == '\n' || c == '\r' || c == '\t') c = ' ';
    return trim(s);
}

static string lower(string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static bool iequals(const string& a, const string& b) { return lower(a) == lower(b); }

static vector<string> split(const string& s, char delim) {
    vector<string> out;
    string cur;
    for (char c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

static string fmt(double v) {
    char b[48];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}

static bool toInt(const string& s, int& out) {
    string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(t.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = (int)v;
    return true;
}

static bool toDouble(const string& s, double& out) {
    string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if (*end != '\0') return false;
    out = v;
    return true;
}

static string timeFmt(const char* f) {
    time_t t = time(nullptr);
    char b[64];
    std::strftime(b, sizeof b, f, localtime(&t));
    return b;
}
static string nowStamp() { return timeFmt("%Y-%m-%d %H:%M:%S"); }
static string todayDate() { return timeFmt("%Y-%m-%d"); }
static string nowHHMM() { return timeFmt("%H:%M"); }
static string currentSlot() { return timeFmt("%H:00"); }

// Accepts both the new "YYYY-MM-DD HH:MM:SS" and the legacy ctime() format
// ("Fri May 09 18:14:31 2025") written by the console version.
static string dateOf(const string& ts) {
    if (ts.size() >= 10 && std::isdigit((unsigned char)ts[0]) && ts[4] == '-') return ts.substr(0, 10);
    char mon[8] = {0};
    int d = 0, y = 0;
    if (std::sscanf(ts.c_str(), "%*3s %3s %d %*d:%*d:%*d %d", mon, &d, &y) == 3) {
        static const char* M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; ++i) {
            if (std::strcmp(mon, M[i]) == 0) {
                char b[16];
                std::snprintf(b, sizeof b, "%04d-%02d-%02d", y, i + 1, d);
                return b;
            }
        }
    }
    return ts;
}

// ---- SHA-256 (used for salted password hashing) ----------------------------
static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static string sha256Hex(const string& data) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    string msg = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    msg.push_back((char)0x80);
    while (msg.size() % 64 != 56) msg.push_back((char)0);
    for (int i = 7; i >= 0; --i) msg.push_back((char)((bits >> (i * 8)) & 0xff));

    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)(uint8_t)msg[off + 4 * i] << 24) | ((uint32_t)(uint8_t)msg[off + 4 * i + 1] << 16) |
                   ((uint32_t)(uint8_t)msg[off + 4 * i + 2] << 8) | ((uint32_t)(uint8_t)msg[off + 4 * i + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    char buf[65];
    for (int i = 0; i < 8; ++i) std::snprintf(buf + i * 8, 9, "%08x", (unsigned)h[i]);
    return string(buf, 64);
}

static string randomSalt() {
    static bool seeded = false;
    if (!seeded) {
        std::srand((unsigned)time(nullptr) ^ (unsigned)GetTickCount() ^ ((unsigned)GetCurrentProcessId() << 8));
        seeded = true;
    }
    char b[17];
    for (int i = 0; i < 16; ++i) b[i] = "0123456789abcdef"[std::rand() % 16];
    b[16] = '\0';
    return b;
}

// Stored format:  <salt>$<sha256(salt + password)>
static string makePasswordHash(const string& pw) {
    string salt = randomSalt();
    return salt + "$" + sha256Hex(salt + pw);
}

static bool checkPassword(const string& pw, const string& stored) {
    size_t p = stored.find('$');
    if (p == string::npos) return false;
    return sha256Hex(stored.substr(0, p) + pw) == stored.substr(p + 1);
}

// ============================================================================
//  2. Domain model (encapsulation)
// ============================================================================
class MenuItem {
    int id_ = 0;
    string name_, category_;
    double price_ = 0.0;
    int stock_ = 0;

public:
    MenuItem() = default;
    MenuItem(int id, string name, string category, double price, int stock)
        : id_(id), name_(std::move(name)), category_(std::move(category)), price_(price), stock_(stock) {}

    int id() const { return id_; }
    const string& name() const { return name_; }
    const string& category() const { return category_; }
    double price() const { return price_; }
    int stock() const { return stock_; }
    void setName(const string& v) { name_ = v; }
    void setCategory(const string& v) { category_ = v; }
    void setPrice(double v) { price_ = v; }
    void setStock(int v) { stock_ = v; }

    string toLine() const {
        return std::to_string(id_) + "," + name_ + "," + category_ + "," + fmt(price_) + "," + std::to_string(stock_);
    }
    static bool fromLine(const string& line, MenuItem& out) {
        vector<string> p = split(line, ',');
        if (p.size() < 5) return false;
        int id, stock;
        double price;
        if (!toInt(p[0], id) || !toDouble(p[3], price) || !toInt(p[4], stock)) return false;
        out = MenuItem(id, trim(p[1]), trim(p[2]), price, stock);
        return true;
    }
};

struct OrderLine {
    string name;
    int qty;
    double unit;
    double subtotal;
};

class Order {
    int id_ = 0, table_ = 0;
    string customer_, address_, phone_, type_ = "Takeaway", status_ = "Pending";
    string timestamp_, receipt_, owner_;
    double total_ = 0.0;
    vector<OrderLine> lines_;

    void recalc() {
        total_ = 0;
        for (const OrderLine& l : lines_) total_ += l.subtotal;
    }

public:
    int id() const { return id_; }
    int table() const { return table_; }
    const string& customer() const { return customer_; }
    const string& address() const { return address_; }
    const string& phone() const { return phone_; }
    const string& type() const { return type_; }
    const string& status() const { return status_; }
    const string& timestamp() const { return timestamp_; }
    const string& receipt() const { return receipt_; }
    const string& owner() const { return owner_; }
    double total() const { return total_; }
    const vector<OrderLine>& lines() const { return lines_; }

    void setId(int v) { id_ = v; }
    void setTable(int v) { table_ = v; }
    void setCustomer(const string& v) { customer_ = v; }
    void setAddress(const string& v) { address_ = v; }
    void setPhone(const string& v) { phone_ = v; }
    void setType(const string& v) { type_ = v; }
    void setStatus(const string& v) { status_ = v; }
    void setTimestamp(const string& v) { timestamp_ = v; }
    void setReceipt(const string& v) { receipt_ = v; }
    void setOwner(const string& v) { owner_ = v; }

    void addLine(const string& name, int qty, double unit) {
        for (OrderLine& l : lines_) {
            if (l.name == name) {
                l.qty += qty;
                l.subtotal = l.qty * l.unit;
                recalc();
                return;
            }
        }
        lines_.push_back(OrderLine{name, qty, unit, qty * unit});
        recalc();
    }
    void removeLine(size_t i) {
        if (i < lines_.size()) {
            lines_.erase(lines_.begin() + (long)i);
            recalc();
        }
    }
    void clearLines() {
        lines_.clear();
        total_ = 0;
    }
    int qtyOf(const string& name) const {
        for (const OrderLine& l : lines_)
            if (l.name == name) return l.qty;
        return 0;
    }
    // Fill missing prices (orders loaded from file only store name:qty). Total is untouched.
    void priceFrom(const vector<MenuItem>& menu) {
        for (OrderLine& l : lines_) {
            if (l.subtotal > 0) continue;
            for (const MenuItem& m : menu) {
                if (m.name() == l.name) {
                    l.unit = m.price();
                    l.subtotal = m.price() * l.qty;
                    break;
                }
            }
        }
    }

    string itemsText() const {
        string s;
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (i) s += ", ";
            s += lines_[i].name + " x" + std::to_string(lines_[i].qty);
        }
        return s;
    }

    // id,name,address,phone,type,table,status,timestamp,total,receipt,items,owner
    // (first 11 fields are identical to the console version; "owner" is new and optional)
    string toLine() const {
        string items;
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (i) items += ";";
            items += lines_[i].name + ":" + std::to_string(lines_[i].qty);
        }
        return std::to_string(id_) + "," + customer_ + "," + address_ + "," + phone_ + "," + type_ + "," +
               std::to_string(table_) + "," + status_ + "," + timestamp_ + "," + fmt(total_) + "," + receipt_ + "," +
               items + "," + owner_;
    }

    static bool fromLine(const string& line, Order& o) {
        vector<string> p = split(line, ',');
        if (p.size() < 11) return false;
        int id = 0, table = 0;
        double total = 0;
        toInt(p[0], id);
        toInt(p[5], table);
        toDouble(p[8], total);
        o = Order();
        o.id_ = id;
        o.customer_ = trim(p[1]);
        o.address_ = trim(p[2]);
        o.phone_ = trim(p[3]);
        o.type_ = trim(p[4]);
        o.table_ = table;
        o.status_ = trim(p[6]);
        o.timestamp_ = trim(p[7]);
        o.total_ = total;
        o.receipt_ = trim(p[9]);
        for (const string& tok : split(p[10], ';')) {
            size_t pos = tok.rfind(':');  // item names may contain ':' (e.g. "Offer 1: ...")
            if (pos == string::npos) continue;
            int qty = 0;
            if (!toInt(tok.substr(pos + 1), qty)) continue;
            o.lines_.push_back(OrderLine{trim(tok.substr(0, pos)), qty, 0.0, 0.0});
        }
        if (p.size() > 11) o.owner_ = trim(p[11]);
        return true;
    }

    string receiptText() const {
        string s = "=== ORDER RECEIPT ===\n";
        s += "Order #: " + std::to_string(id_) + "\n";
        s += "Date: " + timestamp_ + "\n";
        s += "Customer Name: " + customer_ + "\n";
        s += "Address: " + address_ + "\n";
        s += "Phone: " + phone_ + "\n";
        s += "Order Type: " + type_ + "\n";
        if (type_ == "Dine-In") s += "Table: " + std::to_string(table_) + "\n";
        s += "\nItems Ordered:\n";
        for (const OrderLine& l : lines_) {
            s += "- " + l.name + " x" + std::to_string(l.qty);
            if (l.subtotal > 0) s += " = " + fmt(l.subtotal) + " EGP";
            s += "\n";
        }
        s += "\nTotal Amount: " + fmt(total_) + " EGP\n====================\n";
        return s;
    }
};

class Reservation {
    int id_ = 0, table_ = 0, orderId_ = 0;
    string customer_, date_, time_;

public:
    int id() const { return id_; }
    int table() const { return table_; }
    int orderId() const { return orderId_; }
    const string& customer() const { return customer_; }
    const string& date() const { return date_; }
    const string& time() const { return time_; }
    void setId(int v) { id_ = v; }
    void setTable(int v) { table_ = v; }
    void setOrderId(int v) { orderId_ = v; }
    void setCustomer(const string& v) { customer_ = v; }
    void setDate(const string& v) { date_ = v; }
    void setTime(const string& v) { time_ = v; }

    string toLine() const {
        return std::to_string(id_) + "," + customer_ + "," + std::to_string(table_) + "," + date_ + "," + time_ + "," +
               std::to_string(orderId_);
    }
    static bool fromLine(const string& line, Reservation& r) {
        vector<string> p = split(line, ',');
        if (p.size() < 5) return false;
        int id, table, oid = 0;
        if (!toInt(p[0], id) || !toInt(p[2], table)) return false;
        if (p.size() > 5) toInt(p[5], oid);
        r = Reservation();
        r.id_ = id;
        r.customer_ = trim(p[1]);
        r.table_ = table;
        r.date_ = trim(p[3]);
        r.time_ = trim(p[4]);
        r.orderId_ = oid;
        return true;
    }
};

// ============================================================================
//  3. Persistence (plain text files next to the executable)
// ============================================================================
struct UserRecord {
    string username, hash, role, email;
};

struct DefaultItem {
    int id;
    const char* name;
    const char* cat;
    double price;
};
static const DefaultItem DEFAULT_MENU[] = {
    {1, "Ranch Pizza", "Pizzas", 75},
    {2, "BBQ Pizza", "Pizzas", 80},
    {3, "Margherita Pizza", "Pizzas", 70},
    {4, "Pepperoni Pizza", "Pizzas", 50},
    {5, "Chicken Pizza", "Pizzas", 55},
    {6, "Cheese Pizza", "Pizzas", 40},
    {7, "Twister Sandwich", "Sandwiches", 35},
    {8, "Chicken Fillet Sandwich", "Sandwiches", 40},
    {9, "Zinger Sandwich", "Sandwiches", 30},
    {10, "Grand Chicken Sandwich", "Sandwiches", 30},
    {11, "3 Pieces Fried Chicken", "Fried Chicken", 35},
    {12, "9 Pieces Fried Chicken", "Fried Chicken", 25},
    {13, "French Fries", "Side Items", 15},
    {14, "Regular Pepsi", "Drinks", 15},
    {15, "Diet Pepsi", "Drinks", 15},
    {16, "Orange Juice", "Drinks", 10},
    {17, "12 Pieces Fried Chicken", "Fried Chicken", 180},
    {18, "Mineral Water", "Drinks", 10},
    {19, "7UP", "Drinks", 15},
    {20, "Fanta", "Drinks", 15},
    {21, "Offer 1: 2 Pizzas + Pepsi", "Offers", 140},
    {22, "Offer 2: 1 Pizza + Twister + Pepsi", "Offers", 110},
    {23, "Offer 3: Fried Chicken + Twister + Fries + Pepsi", "Offers", 130}};

class Database {
public:
    static const char* USERS() { return "users.txt"; }
    static const char* MENU() { return "menu.txt"; }
    static const char* ORDERS() { return "orders.txt"; }
    static const char* RESERVATIONS() { return "reservations.txt"; }

    static vector<string> readLines(const char* path) {
        vector<string> out;
        std::ifstream in(path);
        string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (trim(line).empty()) continue;
            out.push_back(line);
        }
        return out;
    }
    static void writeLines(const char* path, const vector<string>& lines) {
        std::ofstream out(path, std::ios::trunc);
        for (const string& l : lines) out << l << "\n";
    }

    // ---- menu ----
    static vector<MenuItem> loadMenu() {
        vector<MenuItem> v;
        for (const string& l : readLines(MENU())) {
            MenuItem m;
            if (MenuItem::fromLine(l, m)) v.push_back(m);
        }
        return v;
    }
    static void saveMenu(const vector<MenuItem>& v) {
        vector<string> lines;
        for (const MenuItem& m : v) lines.push_back(m.toLine());
        writeLines(MENU(), lines);
    }

    // ---- orders ----
    static vector<Order> loadOrders() {
        vector<Order> v;
        int mx = 0;
        for (const string& l : readLines(ORDERS())) {
            Order o;
            if (Order::fromLine(l, o)) {
                v.push_back(o);
                mx = std::max(mx, o.id());
            }
        }
        // The console version restarted numbering at 1 on every run -> repair duplicates.
        std::set<int> seen;
        for (Order& o : v) {
            if (o.id() <= 0 || seen.count(o.id())) o.setId(++mx);
            seen.insert(o.id());
        }
        return v;
    }
    static void saveOrders(const vector<Order>& v) {
        vector<string> lines;
        for (const Order& o : v) lines.push_back(o.toLine());
        writeLines(ORDERS(), lines);
    }
    static int nextOrderId(const vector<Order>& v) {
        int mx = 0;
        for (const Order& o : v) mx = std::max(mx, o.id());
        return mx + 1;
    }
    static string receiptPath(int id) { return "receipts\\receipt_" + std::to_string(id) + ".txt"; }
    static void writeReceipt(const Order& o) {
        std::ofstream out(o.receipt(), std::ios::trunc);
        out << o.receiptText();
    }
    // Receipt text for display: the saved file if it belongs to this order, else rebuilt.
    static string receiptFor(Order o) {
        std::ifstream in(o.receipt());
        if (in) {
            string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (txt.find("Order #: " + std::to_string(o.id()) + "\n") != string::npos) return txt;
        }
        o.priceFrom(loadMenu());
        return o.receiptText();
    }

    // ---- reservations ----
    static vector<Reservation> loadReservations() {
        vector<Reservation> v;
        for (const string& l : readLines(RESERVATIONS())) {
            Reservation r;
            if (Reservation::fromLine(l, r)) v.push_back(r);
        }
        return v;
    }
    static void saveReservations(const vector<Reservation>& v) {
        vector<string> lines;
        for (const Reservation& r : v) lines.push_back(r.toLine());
        writeLines(RESERVATIONS(), lines);
    }
    static int nextReservationId(const vector<Reservation>& v) {
        int mx = 0;
        for (const Reservation& r : v) mx = std::max(mx, r.id());
        return mx + 1;
    }
    static bool tableFree(const vector<Reservation>& v, int table, const string& date, const string& time) {
        for (const Reservation& r : v)
            if (r.table() == table && r.date() == date && r.time() == time) return false;
        return true;
    }

    // ---- users ----
    static vector<UserRecord> loadUsers() {
        vector<UserRecord> v;
        for (const string& l : readLines(USERS())) {
            vector<string> p = split(l, ',');
            if (p.size() < 3) continue;
            v.push_back(UserRecord{trim(p[0]), trim(p[1]), trim(p[2]), p.size() > 3 ? trim(p[3]) : ""});
        }
        return v;
    }
    static void saveUsers(const vector<UserRecord>& v) {
        vector<string> lines;
        for (const UserRecord& u : v) lines.push_back(u.username + "," + u.hash + "," + u.role + "," + u.email);
        writeLines(USERS(), lines);
    }

    // Create folders / default data on first run.
    static void init() {
        CreateDirectoryA("receipts", nullptr);

        if (loadMenu().empty()) {
            vector<MenuItem> v;
            for (const DefaultItem& d : DEFAULT_MENU) v.push_back(MenuItem(d.id, d.name, d.cat, d.price, 20));
            saveMenu(v);
        }
        { std::ofstream(ORDERS(), std::ios::app); }
        { std::ofstream(RESERVATIONS(), std::ios::app); }

        vector<UserRecord> users = loadUsers();
        bool legacy = users.empty();
        for (const UserRecord& u : users)
            if (u.hash.find('$') == string::npos) legacy = true;  // old std::hash format
        if (legacy) {
            users.clear();
            users.push_back(UserRecord{"admin", makePasswordHash("admin123"), "admin", "admin@restaurant.local"});
            users.push_back(UserRecord{"user", makePasswordHash("user123"), "customer", "user@example.com"});
            saveUsers(users);
        }
    }
};

// ============================================================================
//  4. Users (inheritance + polymorphism)
// ============================================================================
class User {
protected:
    string name_, email_, passHash_;

public:
    User(string name, string email, string hash)
        : name_(std::move(name)), email_(std::move(email)), passHash_(std::move(hash)) {}
    virtual ~User() = default;
    const string& getName() const { return name_; }
    const string& getEmail() const { return email_; }
    virtual string role() const = 0;
    virtual void openMainWindow(HWND loginWindow) = 0;  // each role shows a different window
};

class Admin : public User {
public:
    using User::User;
    string role() const override { return "Administrator"; }
    void openMainWindow(HWND loginWindow) override;
};

class Customer : public User {
public:
    using User::User;
    string role() const override { return "Customer"; }
    void openMainWindow(HWND loginWindow) override;
};

class Auth {
public:
    static User* login(const string& username, const string& password) {
        for (const UserRecord& r : Database::loadUsers()) {
            if (!iequals(r.username, username) || !checkPassword(password, r.hash)) continue;
            if (r.role == "admin") return new Admin(r.username, r.email, r.hash);
            if (r.role == "customer") return new Customer(r.username, r.email, r.hash);
        }
        return nullptr;
    }

    // Returns an error message, or "" on success.
    static string registerCustomer(const string& usernameIn, const string& password, const string& emailIn) {
        string username = trim(usernameIn), email = trim(emailIn);
        if (username.size() < 3 || username.size() > 20) return "Username must be 3-20 characters.";
        for (char c : username)
            if (!(std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-'))
                return "Username may contain only letters, digits, '_', '.' and '-'.";
        if (password.size() < 6) return "Password must be at least 6 characters.";
        if (email.find('@') == string::npos || email.find('.') == string::npos || email.find(',') != string::npos ||
            email.find(' ') != string::npos)
            return "Please enter a valid email address.";
        vector<UserRecord> users = Database::loadUsers();
        for (const UserRecord& u : users)
            if (iequals(u.username, username)) return "That username is already taken.";
        users.push_back(UserRecord{username, makePasswordHash(password), "customer", email});
        Database::saveUsers(users);
        return "";
    }
};

// ============================================================================
//  5. Win32 helpers + base window
// ============================================================================
static void lvAddCol(HWND h, int idx, const char* text, int width) {
    LVCOLUMNA c;
    std::memset(&c, 0, sizeof c);
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.pszText = const_cast<char*>(text);
    c.cx = width;
    SendMessageA(h, LVM_INSERTCOLUMNA, (WPARAM)idx, (LPARAM)&c);
}
static void lvClear(HWND h) { SendMessageA(h, LVM_DELETEALLITEMS, 0, 0); }
static void lvAddRow(HWND h, const vector<string>& cells) {
    int row = (int)SendMessageA(h, LVM_GETITEMCOUNT, 0, 0);
    LVITEMA it;
    std::memset(&it, 0, sizeof it);
    it.mask = LVIF_TEXT;
    it.iItem = row;
    it.pszText = const_cast<char*>(cells[0].c_str());
    int idx = (int)SendMessageA(h, LVM_INSERTITEMA, 0, (LPARAM)&it);
    for (size_t c = 1; c < cells.size(); ++c) {
        LVITEMA sub;
        std::memset(&sub, 0, sizeof sub);
        sub.iSubItem = (int)c;
        sub.pszText = const_cast<char*>(cells[c].c_str());
        SendMessageA(h, LVM_SETITEMTEXTA, (WPARAM)idx, (LPARAM)&sub);
    }
}
static int lvSel(HWND h) { return (int)SendMessageA(h, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); }
static string lvText(HWND h, int row, int col) {
    char buf[512] = {0};
    LVITEMA it;
    std::memset(&it, 0, sizeof it);
    it.iSubItem = col;
    it.pszText = buf;
    it.cchTextMax = (int)sizeof buf;
    SendMessageA(h, LVM_GETITEMTEXTA, (WPARAM)row, (LPARAM)&it);
    return buf;
}
static int lvInt(HWND h, int row, int col) { return std::atoi(lvText(h, row, col).c_str()); }

// ----------------------------------------------------------------------------
//  Colour theme
// ----------------------------------------------------------------------------
static const COLORREF C_BG = RGB(255, 247, 237);      // warm cream window background
static const COLORREF C_BAND = RGB(62, 39, 35);       // dark chocolate top bar
static const COLORREF C_ACCENT = RGB(244, 124, 32);   // orange accent
static const COLORREF C_TEXT = RGB(52, 40, 36);       // body text
static const COLORREF C_HEAD = RGB(192, 77, 12);      // headings
static const COLORREF C_ROW_ALT = RGB(255, 236, 214); // zebra rows in lists
static const int BAND_H = 60;                         // height of the top bar

static HBRUSH brBg() { static HBRUSH b = CreateSolidBrush(C_BG); return b; }
static HBRUSH brBand() { static HBRUSH b = CreateSolidBrush(C_BAND); return b; }
static COLORREF shade(COLORREF c, int d) {
    auto cl = [](int v) { return std::max(0, std::min(255, v)); };
    return RGB(cl(GetRValue(c) + d), cl(GetGValue(c) + d), cl(GetBValue(c) + d));
}
static bool hasWord(string t, const char* w) {
    for (auto& ch : t) ch = (char)std::tolower((unsigned char)ch);
    return t.find(w) != string::npos;
}
static COLORREF buttonColor(const string& t) {
    if (hasWord(t, "delete") || hasWord(t, "reject") || hasWord(t, "cancel") || hasWord(t, "exit") ||
        hasWord(t, "remove") || hasWord(t, "logout"))
        return RGB(214, 69, 65);   // red
    if (hasWord(t, "clear")) return RGB(120, 113, 108);   // grey
    if (hasWord(t, "register")) return C_ACCENT;          // orange
    if (hasWord(t, "accept") || hasWord(t, "place") || hasWord(t, "add") || hasWord(t, "reserve") ||
        hasWord(t, "login"))
        return RGB(46, 160, 107);  // green
    return RGB(33, 125, 200);      // blue
}

class AppWindow {
public:
    HWND hwnd = nullptr;
    virtual ~AppWindow() {}

    static void registerClass() {
        WNDCLASSEXA wc;
        std::memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hbrBackground = brBg();
        wc.lpszClassName = WND_CLASS;
        RegisterClassExA(&wc);
    }

    bool create(const char* title, int clientW, int clientH, HWND owner = nullptr) {
        ownerWnd = owner;
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT r = {0, 0, clientW, clientH};
        AdjustWindowRect(&r, style, FALSE);
        int w = r.right - r.left, h = r.bottom - r.top;
        int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        HWND hw = CreateWindowExA(0, WND_CLASS, title, style, x, y, w, h, nullptr, nullptr, g_hInst, this);
        if (!hw) return false;
        ShowWindow(hw, SW_SHOW);
        UpdateWindow(hw);
        return true;
    }

protected:
    HWND ownerWnd = nullptr;
    int shown = -1, navCount = 0, curPage = -1;
    vector<vector<HWND>> pages;

    // ---- overridable hooks ----
    virtual void onCreate() = 0;
    virtual void onCommand(int, int) {}
    virtual void onNotify(NMHDR*) {}
    virtual void onPageShown(int) {}
    virtual void onClose() { DestroyWindow(hwnd); }
    virtual void onDestroy() {}
    virtual LRESULT onOther(UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(hwnd, m, w, l); }

    // ---- control factory ----
    HWND ctl(const char* cls, const char* text, DWORD style, int x, int y, int w, int h, int id = 0, DWORD ex = 0) {
        HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, hwnd,
                                 (HMENU)(INT_PTR)id, g_hInst, nullptr);
        if (c) {
            SendMessageA(c, WM_SETFONT, (WPARAM)g_font, TRUE);
            if (curPage >= 0) pages[(size_t)curPage].push_back(c);
        }
        return c;
    }
    HWND label(const char* t, int x, int y, int w, int h = 20, int id = 0) {
        return ctl("STATIC", t, SS_LEFT | SS_NOPREFIX, x, y, w, h, id);
    }
    HWND heading(const char* t, int x, int y, int w, int h, int id = 0) {
        HWND c = label(t, x, y, w, h, id);
        SendMessageA(c, WM_SETFONT, (WPARAM)g_fontBig, TRUE);
        return c;
    }
    HWND edit(int id, int x, int y, int w, int h = 24, DWORD style = 0, const char* text = "") {
        return ctl("EDIT", text, WS_TABSTOP | ES_AUTOHSCROLL | style, x, y, w, h, id, WS_EX_CLIENTEDGE);
    }
    HWND button(const char* t, int id, int x, int y, int w, int h = 30, DWORD style = 0) {
        return ctl("BUTTON", t, WS_TABSTOP | BS_OWNERDRAW | style, x, y, w, h, id);
    }
    HWND combo(int id, int x, int y, int w, bool editable = false, int dropH = 220) {
        return ctl("COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | (editable ? (CBS_DROPDOWN | CBS_AUTOHSCROLL) : CBS_DROPDOWNLIST),
                   x, y, w, dropH, id);
    }
    HWND list(int id, int x, int y, int w, int h, std::initializer_list<std::pair<const char*, int>> cols) {
        HWND c = ctl("SysListView32", "", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, x, y, w,
                     h, id);
        SendMessageA(c, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        SendMessageA(c, LVM_SETBKCOLOR, 0, (LPARAM)RGB(255, 255, 255));
        SendMessageA(c, LVM_SETTEXTBKCOLOR, 0, (LPARAM)RGB(255, 255, 255));
        SendMessageA(c, LVM_SETTEXTCOLOR, 0, (LPARAM)C_TEXT);
        int i = 0;
        for (const auto& col : cols) lvAddCol(c, i++, col.first, col.second);
        return c;
    }
    HWND datePicker(int id, int x, int y, int w, bool minToday) {
        HWND c = ctl("SysDateTimePick32", "", WS_TABSTOP | DTS_SHORTDATECENTURYFORMAT, x, y, w, 26, id);
        if (minToday) {
            SYSTEMTIME rng[2];
            std::memset(rng, 0, sizeof rng);
            GetLocalTime(&rng[0]);
            SendMessageA(c, DTM_SETRANGE, GDTR_MIN, (LPARAM)rng);
        }
        return c;
    }

    // ---- navigation (push-like radio buttons switch between "pages" of controls) ----
    void addNav(const char* text, int index, int x, int w) {
        DWORD st = BS_OWNERDRAW | (index == 0 ? WS_GROUP : 0);
        ctl("BUTTON", text, WS_TABSTOP | st, x, 14, w, 32, ID_NAV + index);
        navCount = std::max(navCount, index + 1);
    }
    void beginPage(int i) {
        curPage = i;
        if ((int)pages.size() <= i) pages.resize((size_t)i + 1);
    }
    void endPage() { curPage = -1; }
    void selectPage(int idx) {
        shown = idx;
        for (size_t p = 0; p < pages.size(); ++p)
            for (HWND h : pages[p]) ShowWindow(h, (int)p == idx ? SW_SHOW : SW_HIDE);
        for (int i = 0; i < navCount; ++i) InvalidateRect(GetDlgItem(hwnd, ID_NAV + i), nullptr, TRUE);
        onPageShown(idx);
    }

    // ---- small helpers ----
    string getText(int id) {
        HWND h = GetDlgItem(hwnd, id);
        int n = GetWindowTextLengthA(h);
        string s((size_t)n + 1, '\0');
        GetWindowTextA(h, &s[0], n + 1);
        s.resize((size_t)n);
        return s;
    }
    void setText(int id, const string& s) { SetDlgItemTextA(hwnd, id, s.c_str()); }
    void cbAdd(int id, const string& s) { SendDlgItemMessageA(hwnd, id, CB_ADDSTRING, 0, (LPARAM)s.c_str()); }
    int cbIndex(int id) { return (int)SendDlgItemMessageA(hwnd, id, CB_GETCURSEL, 0, 0); }
    void cbSelect(int id, int i) { SendDlgItemMessageA(hwnd, id, CB_SETCURSEL, (WPARAM)i, 0); }
    void cbClear(int id) { SendDlgItemMessageA(hwnd, id, CB_RESETCONTENT, 0, 0); }
    string pickedDate(int id) {
        SYSTEMTIME st;
        if (SendDlgItemMessageA(hwnd, id, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) return todayDate();
        char b[40];
        std::snprintf(b, sizeof b, "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth, (int)st.wDay);
        return b;
    }
    void warn(const string& s) { MessageBoxA(hwnd, s.c_str(), APP_TITLE, MB_OK | MB_ICONWARNING); }
    void info(const string& s, const char* title = APP_TITLE) {
        MessageBoxA(hwnd, s.c_str(), title, MB_OK | MB_ICONINFORMATION);
    }
    bool confirm(const string& s) {
        return MessageBoxA(hwnd, s.c_str(), "Please confirm", MB_YESNO | MB_ICONQUESTION) == IDYES;
    }
    void showReceipt(const Order& o) {
        string title = "Receipt - Order #" + std::to_string(o.id());
        info(Database::receiptFor(o), title.c_str());
    }

private:
    static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        AppWindow* self = nullptr;
        if (m == WM_NCCREATE) {
            CREATESTRUCTA* cs = (CREATESTRUCTA*)l;
            self = (AppWindow*)cs->lpCreateParams;
            SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)self);
            if (self) self->hwnd = h;
        } else {
            self = (AppWindow*)GetWindowLongPtrA(h, GWLP_USERDATA);
        }
        if (!self) return DefWindowProcA(h, m, w, l);
        return self->handle(m, w, l);
    }

    void drawButton(DRAWITEMSTRUCT* d) {
        char buf[96];
        GetWindowTextA(d->hwndItem, buf, sizeof buf);
        string t = buf;
        HDC dc = d->hDC;
        RECT rc = d->rcItem;
        RECT wr;
        GetWindowRect(d->hwndItem, &wr);
        MapWindowPoints(nullptr, hwnd, (POINT*)&wr, 2);
        FillRect(dc, &rc, wr.bottom <= BAND_H ? brBand() : brBg());
        int id = (int)d->CtlID;
        COLORREF col = (id >= ID_NAV && id < ID_NAV + 10) ? (id - ID_NAV == shown ? C_ACCENT : RGB(104, 72, 64))
                                                          : buttonColor(t);
        if (d->itemState & ODS_DISABLED) col = RGB(170, 165, 160);
        else if (d->itemState & ODS_SELECTED) col = shade(col, -35);
        HPEN pen = CreatePen(PS_SOLID, 1, col);
        HBRUSH br = CreateSolidBrush(col);
        HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, br);
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen);
        DeleteObject(br);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        HGDIOBJ of = SelectObject(dc, g_font);
        if (d->itemState & ODS_SELECTED) OffsetRect(&rc, 1, 1);
        DrawTextA(dc, t.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, of);
        if (d->itemState & ODS_FOCUS) {
            RECT f = d->rcItem;
            InflateRect(&f, -4, -4);
            DrawFocusRect(dc, &f);
        }
    }

    LRESULT handle(UINT m, WPARAM w, LPARAM l) {
        switch (m) {
            case WM_CREATE:
                onCreate();
                return 0;
            case WM_COMMAND: {
                int id = LOWORD(w), code = HIWORD(w);
                if (id >= ID_NAV && id < ID_NAV + 10 && code == BN_CLICKED) {
                    selectPage(id - ID_NAV);
                    return 0;
                }
                onCommand(id, code);
                return 0;
            }
            case WM_ERASEBKGND: {
                HDC dc = (HDC)w;
                RECT rc;
                GetClientRect(hwnd, &rc);
                FillRect(dc, &rc, brBg());
                RECT band = {0, 0, rc.right, BAND_H};
                FillRect(dc, &band, brBand());
                RECT line = {0, BAND_H - 4, rc.right, BAND_H};
                HBRUSH ac = CreateSolidBrush(C_ACCENT);
                FillRect(dc, &line, ac);
                DeleteObject(ac);
                return 1;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = (HDC)w;
                HWND c = (HWND)l;
                RECT r;
                GetWindowRect(c, &r);
                MapWindowPoints(nullptr, hwnd, (POINT*)&r, 2);
                bool inBand = r.bottom <= BAND_H;
                SetBkMode(dc, TRANSPARENT);
                bool big = (HFONT)SendMessageA(c, WM_GETFONT, 0, 0) == g_fontBig;
                SetTextColor(dc, inBand ? RGB(255, 255, 255) : (big ? C_HEAD : C_TEXT));
                return (LRESULT)(inBand ? brBand() : brBg());
            }
            case WM_DRAWITEM: {
                DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
                if (d->CtlType != ODT_BUTTON) return FALSE;
                drawButton(d);
                return TRUE;
            }
            case WM_NOTIFY: {
                NMHDR* nm = (NMHDR*)l;
                if (nm->code == NM_CUSTOMDRAW) {
                    char cls[32];
                    GetClassNameA(nm->hwndFrom, cls, sizeof cls);
                    if (!std::strcmp(cls, "SysListView32")) {
                        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)l;
                        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                        if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                            bool sel = SendMessageA(nm->hwndFrom, LVM_GETITEMSTATE, cd->nmcd.dwItemSpec, LVIS_SELECTED) &
                                       LVIS_SELECTED;
                            if (!sel) {
                                cd->clrTextBk = (cd->nmcd.dwItemSpec % 2) ? C_ROW_ALT : RGB(255, 255, 255);
                                cd->clrText = C_TEXT;
                            }
                            return CDRF_NEWFONT;
                        }
                        return CDRF_DODEFAULT;
                    }
                }
                onNotify(nm);
                return 0;
            }
            case WM_CLOSE:
                onClose();
                return 0;
            case WM_DESTROY:
                onDestroy();
                return 0;
            case WM_NCDESTROY: {
                HWND h = hwnd;
                SetWindowLongPtrA(h, GWLP_USERDATA, 0);
                LRESULT r = DefWindowProcA(h, m, w, l);
                delete this;
                return r;
            }
            default:
                return onOther(m, w, l);
        }
    }
};

// ----------------------------------------------------------------------------
//  Login window
// ----------------------------------------------------------------------------
class LoginWindow : public AppWindow {
    std::unique_ptr<User> current;

protected:
    void onCreate() override {
        heading("Restaurant Management System", 30, 20, 360, 34);
        label("Sign in, or create a customer account.", 30, 62, 360);
        label("Username", 30, 100, 360);
        edit(ID_L_USER, 30, 120, 360, 26);
        label("Password", 30, 156, 360);
        edit(ID_L_PASS, 30, 176, 360, 26, ES_PASSWORD);
        label("Email (only needed to register)", 30, 212, 360);
        edit(ID_L_EMAIL, 30, 232, 360, 26);
        button("Login", ID_L_LOGIN, 30, 282, 170, 36, BS_DEFPUSHBUTTON);
        button("Register", ID_L_REGISTER, 220, 282, 170, 36);
        button("Exit", ID_L_EXIT, 125, 330, 170, 32);
        SetFocus(GetDlgItem(hwnd, ID_L_USER));
    }

    void onCommand(int id, int code) override {
        if (code != BN_CLICKED) return;
        if (id == ID_L_LOGIN) doLogin();
        else if (id == ID_L_REGISTER) doRegister();
        else if (id == ID_L_EXIT) DestroyWindow(hwnd);
    }

    void onDestroy() override { PostQuitMessage(0); }

    LRESULT onOther(UINT m, WPARAM w, LPARAM l) override {
        if (m == DM_GETDEFID) return MAKELONG(ID_L_LOGIN, DC_HASDEFID);  // Enter = Login
        if (m == WM_APP_LOGOUT) {
            current.reset();
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            SetFocus(GetDlgItem(hwnd, ID_L_PASS));
            return 0;
        }
        if (m == DM_GETDEFID) return MAKELONG(ID_L_LOGIN, DC_HASDEFID);  // Enter = Login
        return AppWindow::onOther(m, w, l);
    }

private:
    void doLogin() {
        string u = trim(getText(ID_L_USER)), p = getText(ID_L_PASS);
        if (u.empty() || p.empty()) {
            warn("Please enter your username and password.");
            return;
        }
        User* usr = Auth::login(u, p);
        if (!usr) {
            MessageBoxA(hwnd, "Invalid username or password.", "Login failed", MB_OK | MB_ICONERROR);
            return;
        }
        current.reset(usr);
        setText(ID_L_PASS, "");
        ShowWindow(hwnd, SW_HIDE);
        current->openMainWindow(hwnd);
    }

    void doRegister() {
        string err = Auth::registerCustomer(getText(ID_L_USER), getText(ID_L_PASS), getText(ID_L_EMAIL));
        if (!err.empty()) {
            warn(err);
            return;
        }
        info("Account created. You can sign in now.");
        setText(ID_L_PASS, "");
        setText(ID_L_EMAIL, "");
    }
};

// ----------------------------------------------------------------------------
//  Admin window
// ----------------------------------------------------------------------------
class AdminWindow : public AppWindow {
    User* user;
    HWND amList = nullptr, aoList = nullptr, arList = nullptr, asList = nullptr;

public:
    explicit AdminWindow(User* u) : user(u) {}

protected:
    void onCreate() override {
        heading(("Admin dashboard - " + user->getName()).c_str(), 20, 12, 420, 32);
        addNav("Menu", 0, 450, 130);
        addNav("Orders", 1, 585, 130);
        addNav("Reservations", 2, 720, 130);
        addNav("Daily Sales", 3, 855, 130);
        button("Logout", ID_LOGOUT, 990, 14, 70, 32);
        buildMenuPage();
        buildOrdersPage();
        buildReservationsPage();
        buildSalesPage();
        selectPage(1);
        SetTimer(hwnd, 1, 30000, nullptr);
    }

    void onDestroy() override {
        KillTimer(hwnd, 1);
        PostMessageA(ownerWnd, WM_APP_LOGOUT, 0, 0);
    }

    void onPageShown(int i) override {
        if (i == 0) refreshMenu();
        else if (i == 1) refreshOrders();
        else if (i == 2) refreshReservations();
        else if (i == 3) refreshSales();
    }

    LRESULT onOther(UINT m, WPARAM w, LPARAM l) override {
        if (m == WM_TIMER && shown == 2) refreshReservations();
        return AppWindow::onOther(m, w, l);
    }

    void onNotify(NMHDR* nm) override {
        if ((int)nm->idFrom == ID_AM_LIST && nm->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* v = (NMLISTVIEW*)nm;
            if ((v->uChanged & LVIF_STATE) && (v->uNewState & LVIS_SELECTED)) fillForm(v->iItem);
        } else if ((int)nm->idFrom == ID_AS_DATE && nm->code == DTN_DATETIMECHANGE) {
            refreshSales();
        }
    }

    void onCommand(int id, int code) override {
        if (code == BN_CLICKED) {
            switch (id) {
                case ID_LOGOUT: DestroyWindow(hwnd); break;
                case ID_AM_ADD: addItem(); break;
                case ID_AM_UPDATE: updateItem(); break;
                case ID_AM_DELETE: deleteItem(); break;
                case ID_AM_CLEAR: clearForm(); break;
                case ID_AO_REFRESH: refreshOrders(); break;
                case ID_AO_ACCEPT: processOrder(true); break;
                case ID_AO_REJECT: processOrder(false); break;
                case ID_AO_RECEIPT: showSelectedReceipt(); break;
                case ID_AR_REFRESH: refreshReservations(); break;
                case ID_AR_CANCEL: cancelReservation(); break;
                case ID_AS_SHOW: refreshSales(); break;
                default: break;
            }
        } else if (code == EN_CHANGE && id == ID_AM_SEARCH) {
            refreshMenu();
        } else if (code == CBN_SELCHANGE) {
            if (id == ID_AO_FILTER) refreshOrders();
            else if (id == ID_AR_FILTER) refreshReservations();
        }
    }

private:
    // ---------------- page builders ----------------
    void buildMenuPage() {
        beginPage(0);
        label("Search:", 20, 66, 60);
        edit(ID_AM_SEARCH, 85, 62, 250);
        amList = list(ID_AM_LIST, 20, 94, 1040, 300,
                      {{"ID", 60}, {"Name", 350}, {"Category", 170}, {"Price (EGP)", 120}, {"Stock", 120}});
        label("Name", 20, 410, 100);
        label("Category", 380, 410, 100);
        label("Price (EGP)", 600, 410, 100);
        label("Stock", 720, 410, 100);
        edit(ID_AM_NAME, 20, 432, 350);
        combo(ID_AM_CAT, 380, 432, 210, true);
        edit(ID_AM_PRICE, 600, 432, 110);
        edit(ID_AM_STOCK, 720, 432, 110);
        button("Add New Item", ID_AM_ADD, 20, 476, 140);
        button("Update Selected", ID_AM_UPDATE, 170, 476, 160);
        button("Delete Selected", ID_AM_DELETE, 340, 476, 160);
        button("Clear Form", ID_AM_CLEAR, 510, 476, 120);
        endPage();
    }
    void buildOrdersPage() {
        beginPage(1);
        label("Status:", 20, 66, 55);
        combo(ID_AO_FILTER, 80, 62, 150);
        cbAdd(ID_AO_FILTER, "All");
        cbAdd(ID_AO_FILTER, "Pending");
        cbAdd(ID_AO_FILTER, "Accepted");
        cbAdd(ID_AO_FILTER, "Rejected");
        cbSelect(ID_AO_FILTER, 1);
        button("Refresh", ID_AO_REFRESH, 245, 60, 100, 28);
        aoList = list(ID_AO_LIST, 20, 96, 1040, 420,
                      {{"ID", 50}, {"Customer", 150}, {"Type", 80}, {"Table", 55}, {"Status", 85},
                       {"Date / Time", 150}, {"Total", 90}, {"Items", 360}});
        button("Accept Order", ID_AO_ACCEPT, 20, 526, 140);
        button("Reject Order", ID_AO_REJECT, 170, 526, 140);
        button("Show Receipt", ID_AO_RECEIPT, 320, 526, 160);
        endPage();
    }
    void buildReservationsPage() {
        beginPage(2);
        label("Show:", 20, 66, 45);
        combo(ID_AR_FILTER, 70, 62, 170);
        cbAdd(ID_AR_FILTER, "All reservations");
        cbAdd(ID_AR_FILTER, "Today only");
        cbAdd(ID_AR_FILTER, "Upcoming");
        cbSelect(ID_AR_FILTER, 0);
        button("Refresh", ID_AR_REFRESH, 255, 60, 100, 28);
        arList = list(ID_AR_LIST, 20, 96, 1040, 380,
                      {{"ID", 60}, {"Customer", 250}, {"Table", 90}, {"Date", 160}, {"Time", 120}, {"Linked order", 140}});
        label("", 20, 488, 1040, 22, ID_AR_FREE);
        button("Cancel Reservation", ID_AR_CANCEL, 20, 520, 190);
        endPage();
    }
    void buildSalesPage() {
        beginPage(3);
        label("Date:", 20, 66, 45);
        datePicker(ID_AS_DATE, 70, 60, 170, false);
        button("Show", ID_AS_SHOW, 255, 60, 100, 28);
        asList = list(ID_AS_LIST, 20, 96, 1040, 380,
                      {{"Order", 70}, {"Customer", 250}, {"Type", 110}, {"Date / Time", 200}, {"Total (EGP)", 130}});
        heading("", 20, 492, 1040, 30, ID_AS_SUMMARY);
        endPage();
    }

    // ---------------- menu management ----------------
    void refreshMenu() {
        vector<MenuItem> items = Database::loadMenu();
        string q = lower(trim(getText(ID_AM_SEARCH)));
        lvClear(amList);
        for (const MenuItem& it : items) {
            if (!q.empty() && lower(it.name()).find(q) == string::npos && lower(it.category()).find(q) == string::npos)
                continue;
            lvAddRow(amList, {std::to_string(it.id()), it.name(), it.category(), fmt(it.price()),
                              std::to_string(it.stock())});
        }
        string keep = getText(ID_AM_CAT);
        cbClear(ID_AM_CAT);
        std::set<string> seen;
        for (const MenuItem& it : items)
            if (seen.insert(lower(it.category())).second) cbAdd(ID_AM_CAT, it.category());
        setText(ID_AM_CAT, keep);
    }
    void fillForm(int row) {
        setText(ID_AM_NAME, lvText(amList, row, 1));
        setText(ID_AM_CAT, lvText(amList, row, 2));
        setText(ID_AM_PRICE, lvText(amList, row, 3));
        setText(ID_AM_STOCK, lvText(amList, row, 4));
    }
    void clearForm() {
        setText(ID_AM_NAME, "");
        setText(ID_AM_CAT, "");
        setText(ID_AM_PRICE, "");
        setText(ID_AM_STOCK, "");
    }
    bool readForm(string& name, string& cat, double& price, int& stock) {
        name = clean(getText(ID_AM_NAME));
        cat = clean(getText(ID_AM_CAT));
        if (name.empty() || cat.empty()) {
            warn("Name and category are required.");
            return false;
        }
        if (!toDouble(getText(ID_AM_PRICE), price) || price <= 0) {
            warn("Enter a valid price greater than 0.");
            return false;
        }
        if (!toInt(getText(ID_AM_STOCK), stock) || stock < 0) {
            warn("Enter a valid stock quantity (0 or more).");
            return false;
        }
        return true;
    }
    void addItem() {
        string name, cat;
        double price;
        int stock;
        if (!readForm(name, cat, price, stock)) return;
        vector<MenuItem> items = Database::loadMenu();
        int mx = 0;
        for (const MenuItem& it : items) {
            if (iequals(it.name(), name)) {
                warn("An item with this name already exists.");
                return;
            }
            mx = std::max(mx, it.id());
        }
        items.push_back(MenuItem(mx + 1, name, cat, price, stock));
        Database::saveMenu(items);
        clearForm();
        refreshMenu();
    }
    void updateItem() {
        int row = lvSel(amList);
        if (row < 0) {
            warn("Select an item in the list first.");
            return;
        }
        int id = lvInt(amList, row, 0);
        string name, cat;
        double price;
        int stock;
        if (!readForm(name, cat, price, stock)) return;
        vector<MenuItem> items = Database::loadMenu();
        for (const MenuItem& it : items)
            if (it.id() != id && iequals(it.name(), name)) {
                warn("Another item already uses this name.");
                return;
            }
        for (MenuItem& it : items) {
            if (it.id() == id) {
                it.setName(name);
                it.setCategory(cat);
                it.setPrice(price);
                it.setStock(stock);
            }
        }
        Database::saveMenu(items);
        refreshMenu();
    }
    void deleteItem() {
        int row = lvSel(amList);
        if (row < 0) {
            warn("Select an item in the list first.");
            return;
        }
        int id = lvInt(amList, row, 0);
        if (!confirm("Delete \"" + lvText(amList, row, 1) + "\" from the menu?")) return;
        vector<MenuItem> items = Database::loadMenu();
        items.erase(std::remove_if(items.begin(), items.end(), [id](const MenuItem& m) { return m.id() == id; }),
                    items.end());
        Database::saveMenu(items);
        clearForm();
        refreshMenu();
    }

    // ---------------- orders ----------------
    void refreshOrders() {
        int f = cbIndex(ID_AO_FILTER);
        static const char* const names[] = {"", "Pending", "Accepted", "Rejected"};
        vector<Order> orders = Database::loadOrders();
        std::sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) { return a.id() > b.id(); });
        lvClear(aoList);
        for (const Order& o : orders) {
            if (f > 0 && o.status() != names[f]) continue;
            lvAddRow(aoList, {std::to_string(o.id()), o.customer(), o.type(),
                              o.table() > 0 ? std::to_string(o.table()) : "-", o.status(), o.timestamp(),
                              fmt(o.total()), o.itemsText()});
        }
    }
    bool findOrder(const vector<Order>& v, int id, size_t& idx) {
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i].id() == id) {
                idx = i;
                return true;
            }
        return false;
    }
    void processOrder(bool accept) {
        int row = lvSel(aoList);
        if (row < 0) {
            warn("Select an order first.");
            return;
        }
        int id = lvInt(aoList, row, 0);
        vector<Order> orders = Database::loadOrders();
        size_t oi;
        if (!findOrder(orders, id, oi)) {
            warn("Order not found.");
            return;
        }
        Order& o = orders[oi];
        if (o.status() != "Pending") {
            warn("This order was already " + lower(o.status()) + ".");
            return;
        }
        if (accept) {
            vector<MenuItem> menu = Database::loadMenu();
            for (const OrderLine& l : o.lines())  // check everything first...
                for (const MenuItem& m : menu)
                    if (m.name() == l.name && m.stock() < l.qty) {
                        warn("Not enough stock for \"" + l.name + "\" (available: " + std::to_string(m.stock()) +
                             ", ordered: " + std::to_string(l.qty) + ").");
                        return;
                    }
            for (const OrderLine& l : o.lines())  // ...then deduct
                for (MenuItem& m : menu)
                    if (m.name() == l.name) m.setStock(m.stock() - l.qty);
            Database::saveMenu(menu);
            o.setStatus("Accepted");
        } else {
            o.setStatus("Rejected");
            vector<Reservation> res = Database::loadReservations();
            res.erase(std::remove_if(res.begin(), res.end(), [id](const Reservation& r) { return r.orderId() == id; }),
                      res.end());
            Database::saveReservations(res);  // frees the dine-in table
        }
        Database::saveOrders(orders);
        refreshOrders();
    }
    void showSelectedReceipt() {
        int row = lvSel(aoList);
        if (row < 0) {
            warn("Select an order first.");
            return;
        }
        int id = lvInt(aoList, row, 0);
        vector<Order> orders = Database::loadOrders();
        size_t oi;
        if (findOrder(orders, id, oi)) showReceipt(orders[oi]);
    }

    // ---------------- reservations ----------------
    void refreshReservations() {
        int f = cbIndex(ID_AR_FILTER);
        string today = todayDate();
        vector<Reservation> res = Database::loadReservations();
        std::sort(res.begin(), res.end(), [](const Reservation& a, const Reservation& b) {
            if (a.date() != b.date()) return a.date() < b.date();
            return a.time() < b.time();
        });
        lvClear(arList);
        for (const Reservation& r : res) {
            if (f == 1 && r.date() != today) continue;
            if (f == 2 && r.date() < today) continue;
            lvAddRow(arList, {std::to_string(r.id()), r.customer(), std::to_string(r.table()), r.date(), r.time(),
                              r.orderId() > 0 ? "#" + std::to_string(r.orderId()) : "-"});
        }
        string slot = currentSlot(), freeList;
        for (int t = 1; t <= TABLE_COUNT; ++t)
            if (Database::tableFree(res, t, today, slot)) freeList += (freeList.empty() ? "" : ", ") + std::to_string(t);
        setText(ID_AR_FREE, "Tables free right now (" + slot + " slot): " + (freeList.empty() ? "none" : freeList));
    }
    void cancelReservation() {
        int row = lvSel(arList);
        if (row < 0) {
            warn("Select a reservation first.");
            return;
        }
        int id = lvInt(arList, row, 0);
        if (!confirm("Cancel reservation #" + std::to_string(id) + "?")) return;
        vector<Reservation> res = Database::loadReservations();
        res.erase(std::remove_if(res.begin(), res.end(), [id](const Reservation& r) { return r.id() == id; }), res.end());
        Database::saveReservations(res);
        refreshReservations();
    }

    // ---------------- daily sales ----------------
    void refreshSales() {
        string date = pickedDate(ID_AS_DATE);
        double total = 0;
        int count = 0;
        lvClear(asList);
        for (const Order& o : Database::loadOrders()) {
            if (o.status() != "Accepted" || dateOf(o.timestamp()) != date) continue;
            lvAddRow(asList, {"#" + std::to_string(o.id()), o.customer(), o.type(), o.timestamp(), fmt(o.total())});
            total += o.total();
            ++count;
        }
        setText(ID_AS_SUMMARY, "Accepted orders: " + std::to_string(count) + "      Total sales: " + fmt(total) + " EGP");
    }
};

// ----------------------------------------------------------------------------
//  Customer window
// ----------------------------------------------------------------------------
class CustomerWindow : public AppWindow {
    User* user;
    Order cart;
    HWND menuList = nullptr, cartList = nullptr, tablesList = nullptr, mineList = nullptr, histList = nullptr;

public:
    explicit CustomerWindow(User* u) : user(u) {}

protected:
    void onCreate() override {
        heading(("Welcome, " + user->getName()).c_str(), 20, 12, 400, 32);
        addNav("Order Food", 0, 440, 140);
        addNav("Reserve a Table", 1, 590, 150);
        addNav("My Orders", 2, 750, 140);
        button("Logout", ID_LOGOUT, 1090, 14, 70, 32);
        buildOrderPage();
        buildReservePage();
        buildHistoryPage();
        selectPage(0);
    }

    void onDestroy() override { PostMessageA(ownerWnd, WM_APP_LOGOUT, 0, 0); }

    void onPageShown(int i) override {
        if (i == 0) {
            refreshMenu();
            refreshCart();
            refreshTableCombo();
        } else if (i == 1) {
            refreshTables();
            refreshMine();
        } else if (i == 2) {
            refreshHistory();
        }
    }

    void onNotify(NMHDR* nm) override {
        if ((int)nm->idFrom == ID_CR_DATE && nm->code == DTN_DATETIMECHANGE) refreshTables();
    }

    void onCommand(int id, int code) override {
        if (code == BN_CLICKED) {
            switch (id) {
                case ID_LOGOUT:
                    if (!cart.lines().empty() && !confirm("Your cart is not empty. Log out anyway?")) return;
                    DestroyWindow(hwnd);
                    break;
                case ID_CO_ADD: addToCart(); break;
                case ID_CO_REMOVE: removeFromCart(); break;
                case ID_CO_CLEAR:
                    cart.clearLines();
                    refreshCart();
                    break;
                case ID_CO_PLACE: placeOrder(); break;
                case ID_CR_RESERVE: reserveTable(); break;
                case ID_CR_CANCEL: cancelMine(); break;
                case ID_CH_REFRESH: refreshHistory(); break;
                case ID_CH_RECEIPT: showHistoryReceipt(); break;
                default: break;
            }
        } else if (code == EN_CHANGE && id == ID_CO_SEARCH) {
            refreshMenu();
        } else if (code == CBN_SELCHANGE) {
            if (id == ID_CO_CAT || id == ID_CO_SORT) refreshMenu();
            else if (id == ID_CO_TYPE) refreshTableCombo();
            else if (id == ID_CR_TIME) refreshTables();
        }
    }

private:
    // ---------------- page builders ----------------
    void buildOrderPage() {
        beginPage(0);
        label("Category:", 20, 66, 65);
        combo(ID_CO_CAT, 90, 62, 140);
        cbAdd(ID_CO_CAT, "All");
        cbSelect(ID_CO_CAT, 0);
        label("Sort:", 245, 66, 40);
        combo(ID_CO_SORT, 285, 62, 170);
        cbAdd(ID_CO_SORT, "Default (ID)");
        cbAdd(ID_CO_SORT, "Name (A-Z)");
        cbAdd(ID_CO_SORT, "Price: low to high");
        cbAdd(ID_CO_SORT, "Price: high to low");
        cbSelect(ID_CO_SORT, 0);
        label("Search:", 470, 66, 50);
        edit(ID_CO_SEARCH, 525, 62, 105);
        menuList = list(ID_CO_MENU, 20, 94, 610, 460,
                        {{"ID", 45}, {"Name", 270}, {"Category", 110}, {"Price", 70}, {"Stock", 90}});
        label("Quantity:", 20, 572, 70);
        edit(ID_CO_QTY, 95, 568, 60, 24, ES_NUMBER, "1");
        button("Add to Cart", ID_CO_ADD, 170, 565, 140);

        label("Your cart", 650, 66, 200);
        cartList = list(ID_CO_CART, 650, 90, 510, 210, {{"Item", 250}, {"Qty", 50}, {"Unit", 85}, {"Subtotal", 105}});
        button("Remove Selected", ID_CO_REMOVE, 650, 308, 150, 28);
        button("Clear Cart", ID_CO_CLEAR, 810, 308, 110, 28);
        heading("Total: 0.00 EGP", 650, 346, 510, 28, ID_CO_TOTAL);
        label("Name", 650, 390, 80);
        edit(ID_CO_NAME, 735, 386, 425, 24, 0, user->getName().c_str());
        label("Address", 650, 424, 80);
        edit(ID_CO_ADDR, 735, 420, 425);
        label("Phone", 650, 458, 80);
        edit(ID_CO_PHONE, 735, 454, 425);
        label("Order type", 650, 494, 80);
        combo(ID_CO_TYPE, 735, 490, 150);
        cbAdd(ID_CO_TYPE, "Takeaway");
        cbAdd(ID_CO_TYPE, "Dine-In");
        cbSelect(ID_CO_TYPE, 0);
        combo(ID_CO_TABLE, 895, 490, 265);
        EnableWindow(GetDlgItem(hwnd, ID_CO_TABLE), FALSE);
        button("Place Order", ID_CO_PLACE, 735, 536, 220, 40);
        endPage();
    }
    void buildReservePage() {
        beginPage(1);
        label("Date", 20, 66, 40);
        datePicker(ID_CR_DATE, 65, 62, 170, true);
        label("Time", 255, 66, 40);
        combo(ID_CR_TIME, 300, 62, 110);
        for (const char* s : SLOTS) cbAdd(ID_CR_TIME, s);
        cbSelect(ID_CR_TIME, 0);
        tablesList = list(ID_CR_TABLES, 20, 100, 400, 270, {{"Table", 120}, {"Status for selected time", 250}});
        button("Reserve Selected Table", ID_CR_RESERVE, 20, 384, 230, 36);
        label("Time slots are one hour long (12:00 - 23:00).", 20, 430, 400);

        label("My reservations", 450, 66, 300);
        mineList = list(ID_CR_MINE, 450, 100, 710, 270,
                        {{"ID", 60}, {"Table", 80}, {"Date", 140}, {"Time", 100}, {"Linked order", 140}});
        button("Cancel Selected", ID_CR_CANCEL, 450, 384, 170, 36);
        endPage();
    }
    void buildHistoryPage() {
        beginPage(2);
        histList = list(ID_CH_LIST, 20, 62, 1140, 480,
                        {{"Order", 60}, {"Date / Time", 160}, {"Type", 90}, {"Table", 60}, {"Status", 90},
                         {"Total", 90}, {"Items", 560}});
        button("Refresh", ID_CH_REFRESH, 20, 552, 120);
        button("Show Receipt", ID_CH_RECEIPT, 150, 552, 160);
        endPage();
    }

    // ---------------- menu / cart ----------------
    void rebuildCategories(const vector<MenuItem>& all) {
        string keep = getText(ID_CO_CAT);
        cbClear(ID_CO_CAT);
        cbAdd(ID_CO_CAT, "All");
        std::set<string> seen;
        for (const MenuItem& it : all)
            if (seen.insert(lower(it.category())).second) cbAdd(ID_CO_CAT, it.category());
        int idx = (int)SendDlgItemMessageA(hwnd, ID_CO_CAT, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)keep.c_str());
        cbSelect(ID_CO_CAT, idx < 0 ? 0 : idx);
    }
    void refreshMenu() {
        vector<MenuItem> all = Database::loadMenu();
        rebuildCategories(all);
        string cat = getText(ID_CO_CAT);
        string q = lower(trim(getText(ID_CO_SEARCH)));
        int sort = cbIndex(ID_CO_SORT);
        vector<MenuItem> v;
        for (const MenuItem& it : all) {
            if (!cat.empty() && cat != "All" && !iequals(it.category(), cat)) continue;
            if (!q.empty() && lower(it.name()).find(q) == string::npos) continue;
            v.push_back(it);
        }
        if (sort == 1)
            std::sort(v.begin(), v.end(), [](const MenuItem& a, const MenuItem& b) { return lower(a.name()) < lower(b.name()); });
        else if (sort == 2)
            std::sort(v.begin(), v.end(), [](const MenuItem& a, const MenuItem& b) { return a.price() < b.price(); });
        else if (sort == 3)
            std::sort(v.begin(), v.end(), [](const MenuItem& a, const MenuItem& b) { return a.price() > b.price(); });
        lvClear(menuList);
        for (const MenuItem& it : v)
            lvAddRow(menuList, {std::to_string(it.id()), it.name(), it.category(), fmt(it.price()),
                                it.stock() > 0 ? std::to_string(it.stock()) : "Out of stock"});
    }
    void refreshCart() {
        lvClear(cartList);
        for (const OrderLine& l : cart.lines())
            lvAddRow(cartList, {l.name, std::to_string(l.qty), fmt(l.unit), fmt(l.subtotal)});
        setText(ID_CO_TOTAL, "Total: " + fmt(cart.total()) + " EGP");
    }
    void addToCart() {
        int row = lvSel(menuList);
        if (row < 0) {
            warn("Select an item from the menu first.");
            return;
        }
        int id = lvInt(menuList, row, 0), qty = 0;
        if (!toInt(getText(ID_CO_QTY), qty) || qty <= 0 || qty > 999) {
            warn("Enter a valid quantity (1-999).");
            return;
        }
        for (const MenuItem& m : Database::loadMenu()) {
            if (m.id() != id) continue;
            if (m.stock() <= 0) {
                warn("Sorry, \"" + m.name() + "\" is out of stock.");
                return;
            }
            if (cart.qtyOf(m.name()) + qty > m.stock()) {
                warn("Only " + std::to_string(m.stock()) + " of \"" + m.name() + "\" available.");
                return;
            }
            cart.addLine(m.name(), qty, m.price());
            refreshCart();
            return;
        }
        warn("That item is no longer on the menu.");
        refreshMenu();
    }
    void removeFromCart() {
        int row = lvSel(cartList);
        if (row < 0) {
            warn("Select a cart line first.");
            return;
        }
        cart.removeLine((size_t)row);
        refreshCart();
    }
    void refreshTableCombo() {
        int sel = cbIndex(ID_CO_TABLE);
        if (sel < 0) sel = 0;
        cbClear(ID_CO_TABLE);
        vector<Reservation> res = Database::loadReservations();
        string d = todayDate(), t = currentSlot();
        for (int i = 1; i <= TABLE_COUNT; ++i)
            cbAdd(ID_CO_TABLE, "Table " + std::to_string(i) + (Database::tableFree(res, i, d, t) ? "" : "  (reserved now)"));
        cbSelect(ID_CO_TABLE, sel);
        EnableWindow(GetDlgItem(hwnd, ID_CO_TABLE), cbIndex(ID_CO_TYPE) == 1);
    }

    void placeOrder() {
        if (cart.lines().empty()) {
            warn("Your cart is empty.");
            return;
        }
        string name = clean(getText(ID_CO_NAME)), addr = clean(getText(ID_CO_ADDR)), phone = clean(getText(ID_CO_PHONE));
        if (name.empty()) {
            warn("Please enter your name.");
            return;
        }
        int digits = 0;
        for (char c : phone)
            if (std::isdigit((unsigned char)c)) ++digits;
        if (digits < 7) {
            warn("Please enter a valid phone number.");
            return;
        }
        bool dine = cbIndex(ID_CO_TYPE) == 1;

        vector<MenuItem> menu = Database::loadMenu();
        for (const OrderLine& l : cart.lines()) {
            bool found = false;
            for (const MenuItem& m : menu) {
                if (m.name() != l.name) continue;
                found = true;
                if (m.stock() < l.qty) {
                    warn("Not enough stock for \"" + l.name + "\" (available: " + std::to_string(m.stock()) + ").");
                    return;
                }
            }
            if (!found) {
                warn("\"" + l.name + "\" is no longer on the menu. Please remove it from your cart.");
                return;
            }
        }

        vector<Reservation> res = Database::loadReservations();
        int table = 0;
        if (dine) {
            int sel = cbIndex(ID_CO_TABLE);
            if (sel < 0) {
                warn("Please choose a table.");
                return;
            }
            table = sel + 1;
            if (!Database::tableFree(res, table, todayDate(), currentSlot())) {
                warn("Table " + std::to_string(table) + " is reserved right now. Please choose another table.");
                refreshTableCombo();
                return;
            }
        }

        vector<Order> orders = Database::loadOrders();
        Order o = cart;
        o.setId(Database::nextOrderId(orders));
        o.setCustomer(name);
        o.setAddress(addr);
        o.setPhone(phone);
        o.setType(dine ? "Dine-In" : "Takeaway");
        o.setTable(table);
        o.setStatus("Pending");
        o.setTimestamp(nowStamp());
        o.setOwner(user->getName());
        o.setReceipt(Database::receiptPath(o.id()));
        Database::writeReceipt(o);
        orders.push_back(o);
        Database::saveOrders(orders);

        if (dine) {
            Reservation r;
            r.setId(Database::nextReservationId(res));
            r.setCustomer(user->getName());
            r.setTable(table);
            r.setDate(todayDate());
            r.setTime(currentSlot());
            r.setOrderId(o.id());
            res.push_back(r);
            Database::saveReservations(res);
        }

        info(o.receiptText() + "\nYour order is pending approval by the restaurant.", "Order placed");
        cart.clearLines();
        refreshCart();
        refreshMenu();
        refreshTableCombo();
    }

    // ---------------- reservations ----------------
    void refreshTables() {
        string d = pickedDate(ID_CR_DATE), t = getText(ID_CR_TIME);
        vector<Reservation> res = Database::loadReservations();
        lvClear(tablesList);
        for (int i = 1; i <= TABLE_COUNT; ++i)
            lvAddRow(tablesList, {std::to_string(i), Database::tableFree(res, i, d, t) ? "Available" : "Reserved"});
    }
    void refreshMine() {
        lvClear(mineList);
        vector<Reservation> res = Database::loadReservations();
        std::sort(res.begin(), res.end(), [](const Reservation& a, const Reservation& b) {
            if (a.date() != b.date()) return a.date() < b.date();
            return a.time() < b.time();
        });
        for (const Reservation& r : res)
            if (iequals(r.customer(), user->getName()))
                lvAddRow(mineList, {std::to_string(r.id()), std::to_string(r.table()), r.date(), r.time(),
                                    r.orderId() > 0 ? "#" + std::to_string(r.orderId()) : "-"});
    }
    void reserveTable() {
        int row = lvSel(tablesList);
        if (row < 0) {
            warn("Select a table from the list first.");
            return;
        }
        int table = lvInt(tablesList, row, 0);
        string d = pickedDate(ID_CR_DATE), t = getText(ID_CR_TIME);
        if (t.empty()) {
            warn("Choose a time slot.");
            return;
        }
        if (d == todayDate() && t <= nowHHMM()) {
            warn("That time slot has already started. Please pick a later one.");
            return;
        }
        vector<Reservation> res = Database::loadReservations();
        if (!Database::tableFree(res, table, d, t)) {
            warn("Table " + std::to_string(table) + " is already reserved at that time.");
            refreshTables();
            return;
        }
        Reservation r;
        r.setId(Database::nextReservationId(res));
        r.setCustomer(user->getName());
        r.setTable(table);
        r.setDate(d);
        r.setTime(t);
        res.push_back(r);
        Database::saveReservations(res);
        info("Table " + std::to_string(table) + " reserved on " + d + " at " + t + ".");
        refreshTables();
        refreshMine();
    }
    void cancelMine() {
        int row = lvSel(mineList);
        if (row < 0) {
            warn("Select one of your reservations first.");
            return;
        }
        if (lvText(mineList, row, 4) != "-") {
            warn("This reservation belongs to a dine-in order and cannot be cancelled here.");
            return;
        }
        int id = lvInt(mineList, row, 0);
        if (!confirm("Cancel reservation #" + std::to_string(id) + "?")) return;
        vector<Reservation> res = Database::loadReservations();
        res.erase(std::remove_if(res.begin(), res.end(),
                                 [&](const Reservation& r) { return r.id() == id && iequals(r.customer(), user->getName()); }),
                  res.end());
        Database::saveReservations(res);
        refreshTables();
        refreshMine();
    }

    // ---------------- order history ----------------
    void refreshHistory() {
        vector<Order> orders = Database::loadOrders();
        std::sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) { return a.id() > b.id(); });
        lvClear(histList);
        for (const Order& o : orders)
            if (iequals(o.owner(), user->getName()))
                lvAddRow(histList, {std::to_string(o.id()), o.timestamp(), o.type(),
                                    o.table() > 0 ? std::to_string(o.table()) : "-", o.status(), fmt(o.total()),
                                    o.itemsText()});
    }
    void showHistoryReceipt() {
        int row = lvSel(histList);
        if (row < 0) {
            warn("Select an order first.");
            return;
        }
        int id = lvInt(histList, row, 0);
        for (const Order& o : Database::loadOrders())
            if (o.id() == id) {
                showReceipt(o);
                return;
            }
    }
};

// Polymorphic entry points: each role opens its own window.
void Admin::openMainWindow(HWND loginWindow) {
    AdminWindow* w = new AdminWindow(this);
    w->create("Restaurant Management - Admin", 1080, 600, loginWindow);
}
void Customer::openMainWindow(HWND loginWindow) {
    CustomerWindow* w = new CustomerWindow(this);
    w->create("Restaurant Management - Customer", 1180, 650, loginWindow);
}

// ============================================================================
//  6. Entry point
// ============================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    g_hInst = hInst;

    // Keep data files next to the executable no matter where it is launched from.
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH)) {
        string p = path;
        size_t s = p.find_last_of("\\/");
        if (s != string::npos) SetCurrentDirectoryA(p.substr(0, s).c_str());
    }

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    g_font = CreateFontA(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_fontBig = CreateFontA(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    Database::init();
    AppWindow::registerClass();

    LoginWindow* login = new LoginWindow();
    login->create("Restaurant Management System - Login", 420, 380);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        HWND top = GetAncestor(msg.hwnd, GA_ROOT);
        if (top && IsDialogMessageA(top, &msg)) continue;  // Tab / Enter / arrow navigation
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    DeleteObject(g_font);
    DeleteObject(g_fontBig);
    return (int)msg.wParam;
}
