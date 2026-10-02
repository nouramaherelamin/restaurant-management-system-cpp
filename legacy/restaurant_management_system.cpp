#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <windows.h>

using namespace std;

const int INITIAL_STOCK = 20;
const string USERS_FILE = "users.txt";
const string MENU_FILE = "menu.txt";
const string ORDERS_FILE = "orders.txt";
const string TABLES_FILE = "tables.txt";
const string RECEIPTS_DIR = "receipts\\";

string getCurrentTime() {
    time_t now = time(0);
    string timeStr = ctime(&now);
    return timeStr.substr(0, timeStr.length() - 1);
}

string hashPassword(const string& password) {
    hash<string> hasher;
    return to_string(hasher(password));
}

bool createDirectory(const string& path) {
    wstring wPath(path.begin(), path.end());
    return CreateDirectoryW(wPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

class MenuItem {
private:
    int id;
    string name;
    string category;
    double price;
    int stock;
public:
    MenuItem() : id(0), price(0.0), stock(0) {}
    MenuItem(int id, string name, string category, double price, int stock)
        : id(id), name(name), category(category), price(price), stock(stock) {}
    
    void displayShort() const {
        cout << left << setw(5) << id << ". " << setw(30) << name << " - " << fixed << setprecision(2) << price << " EGP (Stock: " << stock << ")" << endl;
    }
    
    void saveToFile(ofstream& out) const {
        out << id << "," << name << "," << category << "," << price << "," << stock << endl;
    }
    
    void loadFromFile(string line) {
        stringstream ss(line);
        string idStr, priceStr, stockStr;
        getline(ss, idStr, ',');
        getline(ss, name, ',');
        getline(ss, category, ',');
        getline(ss, priceStr, ',');
        getline(ss, stockStr);
        id = stoi(idStr);
        price = stod(priceStr);
        stock = stoi(stockStr);
    }
    
    int getId() const { return id; }
    string getName() const { return name; }
    string getCategory() const { return category; }
    double getPrice() const { return price; }
    int getStock() const { return stock; }
    void setStock(int newStock) { stock = newStock; }
};

struct CartItem {
    MenuItem item;
    int quantity;
    double subtotal;

    CartItem(const MenuItem& menuItem, int qty) 
        : item(menuItem), quantity(qty), subtotal(menuItem.getPrice() * qty) {}
};

class Order {
private:
    static int nextOrderId;
    int orderId;
    string customerName;
    string address;
    string phone;
    bool isDineIn;
    int tableNumber;
    vector<CartItem> cartItems;
    double total;
    string status;
    string timestamp;
    string receiptPath;

public:
    Order() : orderId(++nextOrderId), isDineIn(false), tableNumber(0), total(0.0), status("Pending") {
        timestamp = getCurrentTime();
    }

    void setCustomerInfo(string name, string addr, string ph) {
        customerName = name;
        address = addr;
        phone = ph;
    }

    void setDiningOption(bool dineIn, int table = 0) {
        isDineIn = dineIn;
        tableNumber = table;
    }

    void addItem(const MenuItem& item, int quantity) {
        for (CartItem& cartItem : cartItems) {
            if (cartItem.item.getId() == item.getId()) {
                cartItem.quantity += quantity;
                cartItem.subtotal += item.getPrice() * quantity;
                total += item.getPrice() * quantity;
                return;
            }
        }
        cartItems.push_back(CartItem(item, quantity));
        total += item.getPrice() * quantity;
    }

    void displayReceipt() const {
        cout << "\n=== ORDER RECEIPT ===\n";
        cout << "Customer Name: " << customerName << "\n";
        cout << "Address: " << address << "\n";
        cout << "Phone: " << phone << "\n";
        cout << "Order Type: " << (isDineIn ? "Dine-In" : "Takeaway") << "\n";
        if (isDineIn) cout << "Table: " << tableNumber << "\n";
        cout << "\nItems Ordered:\n";
        for (const CartItem& cartItem : cartItems) {
            cout << "- " << cartItem.item.getName() << " x" << cartItem.quantity
                 << " = " << fixed << setprecision(2) << cartItem.subtotal << " EGP\n";
        }
        cout << "\nTotal Amount: " << fixed << setprecision(2) << total << " EGP\n";
        cout << "====================\n";
    }

    void generateReceiptFile() {
        receiptPath = RECEIPTS_DIR + "receipt_" + to_string(orderId) + ".txt";
        ofstream fout(receiptPath);
        fout << "=== ORDER RECEIPT ===\n";
        fout << "Customer Name: " << customerName << "\n";
        fout << "Address: " << address << "\n";
        fout << "Phone: " << phone << "\n";
        fout << "Order Type: " << (isDineIn ? "Dine-In" : "Takeaway") << "\n";
        if (isDineIn) fout << "Table: " << tableNumber << "\n";
        fout << "\nItems Ordered:\n";
        for (const CartItem& cartItem : cartItems) {
            fout << "- " << cartItem.item.getName() << " x" << cartItem.quantity
                 << " = " << fixed << setprecision(2) << cartItem.subtotal << " EGP\n";
        }
        fout << "\nTotal Amount: " << fixed << setprecision(2) << total << " EGP\n";
        fout << "====================\n";
        fout.close();
    }

    void saveToFile() {
        generateReceiptFile();
        ofstream fout(ORDERS_FILE, ios::app);
        fout << orderId << "," << customerName << "," << address << "," << phone << ","
             << (isDineIn ? "Dine-In" : "Takeaway") << "," << tableNumber << ","
             << status << "," << timestamp << "," << total << "," << receiptPath;
        fout << ",";
        for (size_t i = 0; i < cartItems.size(); i++) {
            fout << cartItems[i].item.getName() << ":" << cartItems[i].quantity;
            if (i < cartItems.size() - 1) fout << ";";
        }
        fout << endl;
        fout.close();
    }

    bool isEmpty() const { return cartItems.empty(); }
    void clear() { cartItems.clear(); total = 0.0; }
    int getOrderId() const { return orderId; }
    string getCustomerName() const { return customerName; }
};

int Order::nextOrderId = 0;

class User {
protected:
    string username;
    string password;
public:
    User(string u, string p) : username(u), password(p) {}
    virtual void showMenu() = 0;
    string getUsername() const { return username; }
    static string hashPassword(const string& password) {
        hash<string> hasher;
        return to_string(hasher(password));
    }
    static bool verifyPassword(const string& password, const string& hashedPassword) {
        return hashPassword(password) == hashedPassword;
    }
    virtual ~User() {}
};

class Admin : public User {
public:
    Admin(string u, string p) : User(u, p) {}
    
    void showMenu() override {
        while (true) {
            cout << "\nAdmin Menu:\n";
            cout << "1. View Pending Orders\n";
            cout << "2. Process Orders\n";
            cout << "3. Inventory Management\n";
            cout << "4. Daily Sales\n";
            cout << "5. Exit\n";
            cout << "Choice: ";
            int choice;
            cin >> choice;
            if (choice == 1) viewPendingOrders();
            else if (choice == 2) processOrders();
            else if (choice == 3) manageInventory();
            else if (choice == 4) viewDailySales();
            else if (choice == 5) break;
            else cout << "Invalid choice.\n";
        }
    }

private:
    void viewPendingOrders() {
        ifstream fin(ORDERS_FILE);
        string line;
        bool hasPending = false;
        cout << "\nPending Orders:\n";
        cout << string(50, '-') << endl;
        while (getline(fin, line)) {
            if (line.empty()) continue;  // Skip empty lines
            stringstream ss(line);
            string id, name, status;
            getline(ss, id, ',');
            getline(ss, name, ',');
            // Skip address, phone, order type, table number
            for (int i = 0; i < 4; i++) {
                string temp;
                getline(ss, temp, ',');
            }
            getline(ss, status, ',');
            if (status == "Pending") {
                cout << "Order ID: " << id << " | Customer: " << name << endl;
                hasPending = true;
            }
        }
        if (!hasPending) cout << "No pending orders.\n";
        fin.close();
    }

    void processOrders() {
        cout << "Enter order ID to process (0 to return): ";
        int orderId;
        cin >> orderId;
        if (orderId == 0) return;
        ifstream fin(ORDERS_FILE);
        ofstream temp("temp.txt");
        string line;
        bool found = false;
        while (getline(fin, line)) {
            if (line.empty()) continue;  // Skip empty lines
            stringstream ss(line);
            string id;
            getline(ss, id, ',');
            if (stoi(id) == orderId) {
                found = true;
                cout << "Accept (A) or Reject (R)? ";
                char choice;
                cin >> choice;
                string status = (choice == 'A' || choice == 'a') ? "Accepted" : "Rejected";
                if (status == "Accepted") updateStock(line);
                // Find the status field (7th field)
                vector<string> fields;
                string field;
                stringstream lineStream(line);
                while (getline(lineStream, field, ',')) {
                    fields.push_back(field);
                }
                if (fields.size() >= 7) {
                    fields[6] = status;  // Update status field
                    string newLine;
                    for (size_t i = 0; i < fields.size(); i++) {
                        newLine += fields[i];
                        if (i < fields.size() - 1) newLine += ",";
                    }
                    temp << newLine << endl;
                } else {
                    temp << line << endl;
                }
            } else {
                temp << line << endl;
            }
        }
        fin.close();
        temp.close();
        remove(ORDERS_FILE.c_str());
        rename("temp.txt", ORDERS_FILE.c_str());
        if (!found) cout << "Order not found.\n";
    }

    void updateStock(const string& orderLine) {
        stringstream ss(orderLine);
        string temp;
        for (int i = 0; i < 10; i++) getline(ss, temp, ',');
        string items;
        getline(ss, items);
        ifstream fin(MENU_FILE);
        ofstream tempFile("temp_menu.txt");
        string line;
        while (getline(fin, line)) {
            stringstream ss(line);
            string id, name, category, price, stock;
            getline(ss, id, ',');
            getline(ss, name, ',');
            getline(ss, category, ',');
            getline(ss, price, ',');
            getline(ss, stock);
            stringstream itemsSS(items);
            string item;
            bool found = false;
            while (getline(itemsSS, item, ';')) {
                string itemName = item.substr(0, item.find(':'));
                int quantity = stoi(item.substr(item.find(':') + 1));
                if (itemName == name) {
                    int newStock = stoi(stock) - quantity;
                    tempFile << id << "," << name << "," << category << "," << price << "," << newStock << endl;
                    found = true;
                    break;
                }
            }
            if (!found) tempFile << line << endl;
        }
        fin.close();
        tempFile.close();
        remove(MENU_FILE.c_str());
        rename("temp_menu.txt", MENU_FILE.c_str());
    }

    void manageInventory() {
        vector<MenuItem> menuItems;
        ifstream fin(MENU_FILE);
        string line;
        while (getline(fin, line)) {
            MenuItem item;
            item.loadFromFile(line);
            menuItems.push_back(item);
        }
        fin.close();

        while (true) {
            cout << "\nCurrent Inventory:\n";
            cout << string(60, '-') << endl;
            cout << left << setw(5) << "ID" << setw(30) << "Name" << setw(20) << "Category" << setw(10) << "Stock" << endl;
            cout << string(60, '-') << endl;
            
            for (const MenuItem& item : menuItems) {
                cout << left << setw(5) << item.getId() 
                     << setw(30) << item.getName() 
                     << setw(20) << item.getCategory() 
                     << setw(10) << item.getStock() << endl;
            }
            
            cout << "\n1. Update Stock\n";
            cout << "2. Back to Admin Menu\n";
            cout << "Choice: ";
            int choice;
            cin >> choice;
            
            if (choice == 2) break;
            if (choice != 1) {
                cout << "Invalid choice.\n";
                continue;
            }
            
            cout << "Enter item ID to update (0 to cancel): ";
            int itemId;
            cin >> itemId;
            if (itemId == 0) continue;
            
            bool found = false;
            for (MenuItem& item : menuItems) {
                if (item.getId() == itemId) {
                    found = true;
                    cout << "Enter new stock value: ";
                    int newStock;
                    cin >> newStock;
                    if (newStock >= 0) {
                        item.setStock(newStock);
                        cout << "Stock updated successfully.\n";
                    } else {
                        cout << "Invalid stock value.\n";
                    }
                    break;
                }
            }
            
            if (!found) {
                cout << "Item not found.\n";
                continue;
            }
            
            // Save updated inventory
            ofstream fout(MENU_FILE);
            for (const MenuItem& item : menuItems) {
                item.saveToFile(fout);
            }
            fout.close();
        }
    }

    void viewDailySales() {
        string today = getCurrentTime().substr(0, 10); // Get today's date
        ifstream fin(ORDERS_FILE);
        string line;
        double totalSales = 0.0;
        int orderCount = 0;
        
        cout << "\nDaily Sales Report (" << today << "):\n";
        cout << string(50, '-') << endl;
        
        while (getline(fin, line)) {
            if (line.empty()) continue;
            
            stringstream ss(line);
            string id, name, address, phone, orderType, table, status, timestamp, totalStr;
            getline(ss, id, ',');
            getline(ss, name, ',');
            getline(ss, address, ',');
            getline(ss, phone, ',');
            getline(ss, orderType, ',');
            getline(ss, table, ',');
            getline(ss, status, ',');
            getline(ss, timestamp, ',');
            getline(ss, totalStr, ',');
            
            // Check if order is from today and is accepted
            if (timestamp.substr(0, 10) == today && status == "Accepted") {
                double orderTotal = stod(totalStr);
                totalSales += orderTotal;
                orderCount++;
                
                cout << "Order #" << id << " | " << name << " | " 
                     << fixed << setprecision(2) << orderTotal << " EGP\n";
            }
        }
        
        cout << string(50, '-') << endl;
        cout << "Total Orders: " << orderCount << endl;
        cout << "Total Sales: " << fixed << setprecision(2) << totalSales << " EGP\n";
        cout << string(50, '-') << endl;
        
        fin.close();
    }
};

class Customer : public User {
private:
    vector<MenuItem> menuItems;
    Order currentOrder;
    bool tables[10];
public:
    Customer(string u, string p) : User(u, p) {
        for (int i = 0; i < 10; i++) tables[i] = true;
        loadMenu();
        loadTableStatus();
    }

    void loadMenu() {
        menuItems.clear();
        ifstream fin(MENU_FILE);
        string line;
        while (getline(fin, line)) {
            MenuItem item;
            item.loadFromFile(line);
            menuItems.push_back(item);
        }
        fin.close();
    }

    void saveMenu() {
        ofstream fout(MENU_FILE);
        for (const MenuItem& item : menuItems) {
            item.saveToFile(fout);
        }
        fout.close();
    }

    void viewMenu() {
        while (true) {
            cout << "\nMenu Categories:\n";
            cout << "1. Pizzas\n";
            cout << "2. Sandwiches\n";
            cout << "3. Fried Chicken\n";
            cout << "4. Drinks\n";
            cout << "5. Side Items\n";
            cout << "6. Offers\n";
            cout << "7. Back to Main Menu\n";
            cout << "Choice: ";
            int choice;
            cin >> choice;
            if (choice == 7) break;
            string category;
            if (choice == 1) category = "Pizzas";
            else if (choice == 2) category = "Sandwiches";
            else if (choice == 3) category = "Fried Chicken";
            else if (choice == 4) category = "Drinks";
            else if (choice == 5) category = "Side Items";
            else if (choice == 6) category = "Offers";
            else {
                cout << "Invalid choice. Please try again.\n";
                continue;
            }
            while (true) {
                cout << "\nItems in " << category << ":\n";
                for (const MenuItem& item : menuItems) {
                    if (item.getCategory() == category) {
                        item.displayShort();
                    }
                }
                cout << "\n1. Add Item to Cart\n";
                cout << "2. Back to Categories\n";
                cout << "Choice: ";
                int subChoice;
                cin >> subChoice;
                if (subChoice == 2) break;
                if (subChoice != 1) {
                    cout << "Invalid choice. Please try again.\n";
                    continue;
                }
                cout << "Enter item ID: ";
                int itemId;
                cin >> itemId;
                bool found = false;
                for (MenuItem& item : menuItems) {
                    if (item.getId() == itemId && item.getCategory() == category) {
                        found = true;
                        if (item.getStock() <= 0) {
                            cout << "Item out of stock.\n";
                            break;
                        }
                        cout << "Enter quantity: ";
                        int quantity;
                        cin >> quantity;
                        if (quantity <= 0 || quantity > item.getStock()) {
                            cout << "Invalid quantity.\n";
                            break;
                        }
                        currentOrder.addItem(item, quantity);
                        item.setStock(item.getStock() - quantity);
                        saveMenu();
                        cout << "\nAdded to cart!\n";
                        break;
                    }
                }
                if (!found) cout << "Invalid item ID.\n";
            }
        }
    }

    void checkout() {
        if (currentOrder.isEmpty()) {
            cout << "\nCart is empty.\n";
            return;
        }
        string name, address, phone;
        cout << "\nEnter your name: ";
        cin.ignore();
        getline(cin, name);
        cout << "Enter your address: ";
        getline(cin, address);
        cout << "Enter your phone number: ";
        getline(cin, phone);
        currentOrder.setCustomerInfo(name, address, phone);
        cout << "\nOrder Type:\n";
        cout << "1. Takeaway\n";
        cout << "2. Dine-In\n";
        cout << "Choice: ";
        int diningChoice;
        cin >> diningChoice;
        if (diningChoice == 2) {
            cout << "\nAvailable tables:\n";
            bool available = false;
            for (int i = 0; i < 10; i++) {
                if (tables[i]) {
                    cout << "Table " << (i + 1) << ": Available\n";
                    available = true;
                }
            }
            if (!available) {
                cout << "No tables available.\n";
                return;
            }
            cout << "Enter table number (1-10): ";
            int table;
            cin >> table;
            if (table < 1 || table > 10 || !tables[table - 1]) {
                cout << "Invalid or unavailable table.\n";
                return;
            }
            tables[table - 1] = false;
            saveTableStatus();
            currentOrder.setDiningOption(true, table);
        } else {
            currentOrder.setDiningOption(false);
        }
        currentOrder.displayReceipt();
        currentOrder.saveToFile();
        currentOrder.clear();
        cout << "\nThank you for your order! You will be logged out now.\n";
    }

    void loadTableStatus() {
        ifstream fin(TABLES_FILE);
        if (!fin) return;
        string line;
        int i = 0;
        while (getline(fin, line) && i < 10) {
            tables[i] = (line == "1");
            i++;
        }
        fin.close();
    }

    void saveTableStatus() {
        ofstream fout(TABLES_FILE);
        for (int i = 0; i < 10; i++) {
            fout << (tables[i] ? "1" : "0") << endl;
        }
        fout.close();
    }

    void showMenu() override {
        while (true) {
            cout << "\nCustomer Menu:\n";
            cout << "1. View Menu\n";
            cout << "2. Checkout\n";
            cout << "3. Exit\n";
            cout << "Choice: ";
            int choice;
            cin >> choice;
            if (choice == 1) viewMenu();
            else if (choice == 2) {
                checkout();
                break;
            }
            else if (choice == 3) break;
            else cout << "Invalid choice.\n";
        }
    }
};

bool login(string& userType, User*& userPtr) {
    string username, password;
    cout << "\nUsername: ";
    cin >> username;
    cout << "Password: ";
    cin >> password;
    
    // Hash the input password
    string hashedPassword = User::hashPassword(password);
    
    ifstream fin(USERS_FILE);
    if (!fin) {
        cout << "Cannot open users file.\n";
        return false;
    }
    
    string line;
    while (getline(fin, line)) {
        stringstream ss(line);
        string user, storedHash, type;
        getline(ss, user, ',');
        getline(ss, storedHash, ',');
        getline(ss, type);
        
        if (user == username && storedHash == hashedPassword) {
            userType = type;
            if (type == "admin") {
                userPtr = new Admin(username, hashedPassword);
            } else if (type == "customer") {
                userPtr = new Customer(username, hashedPassword);
            } else {
                fin.close();
                return false;
            }
            fin.close();
            cout << "Login successful.\n";
            return true;
        }
    }
    fin.close();
    cout << "Invalid username or password.\n";
    return false;
}

void initializeMenu() {
    ifstream fin(MENU_FILE);
    if (fin.peek() == ifstream::traits_type::eof()) {
        fin.close();
        ofstream fout(MENU_FILE);
        fout << "1,Ranch Pizza,Pizzas,75.00," << INITIAL_STOCK << endl;
        fout << "2,BBQ Pizza,Pizzas,80.00," << INITIAL_STOCK << endl;
        fout << "3,Margherita Pizza,Pizzas,70.00," << INITIAL_STOCK << endl;
        fout << "4,Pepperoni Pizza,Pizzas,50.00," << INITIAL_STOCK << endl;
        fout << "5,Chicken Pizza,Pizzas,55.00," << INITIAL_STOCK << endl;
        fout << "6,Cheese Pizza,Pizzas,40.00," << INITIAL_STOCK << endl;
        fout << "7,Twister Sandwich,Sandwiches,35.00," << INITIAL_STOCK << endl;
        fout << "8,Chicken Fillet Sandwich,Sandwiches,40.00," << INITIAL_STOCK << endl;
        fout << "9,Zinger Sandwich,Sandwiches,30.00," << INITIAL_STOCK << endl;
        fout << "10,Grand Chicken Sandwich,Sandwiches,30.00," << INITIAL_STOCK << endl;
        fout << "11,3 Pieces Fried Chicken,Fried Chicken,35.00," << INITIAL_STOCK << endl;
        fout << "12,9 Pieces Fried Chicken,Fried Chicken,25.00," << INITIAL_STOCK << endl;
        fout << "13,French Fries,Side Items,15.00," << INITIAL_STOCK << endl;
        fout << "14,Regular Pepsi,Drinks,15.00," << INITIAL_STOCK << endl;
        fout << "15,Diet Pepsi,Drinks,15.00," << INITIAL_STOCK << endl;
        fout << "16,Orange Juice,Drinks,10.00," << INITIAL_STOCK << endl;
        fout << "17,12 Pieces Fried Chicken,Fried Chicken,180.00," << INITIAL_STOCK << endl;
        fout << "18,Mineral Water,Drinks,10.00," << INITIAL_STOCK << endl;
        fout << "19,7UP,Drinks,15.00," << INITIAL_STOCK << endl;
        fout << "20,Fanta,Drinks,15.00," << INITIAL_STOCK << endl;
        fout << "21,Offer 1: 2 Pizzas + Pepsi,Offers,140.00," << INITIAL_STOCK << endl;
        fout << "22,Offer 2: 1 Pizza + Twister + Pepsi,Offers,110.00," << INITIAL_STOCK << endl;
        fout << "23,Offer 3: Fried Chicken + Twister + Fries + Pepsi,Offers,130.00," << INITIAL_STOCK << endl;
        fout.close();
    } else {
        fin.close();
    }
}

int main() {
    if (!createDirectory(RECEIPTS_DIR)) {
        cout << "Failed to create receipts directory.\n";
        return 1;
    }
    
    // Initialize users file with correct hashed passwords
    ofstream usersFile(USERS_FILE);
    if (usersFile.is_open()) {
        string adminPass = "2904";
        string userPass = "2904";
        usersFile << "admin," << User::hashPassword(adminPass) << ",admin\n";
        usersFile << "user," << User::hashPassword(userPass) << ",customer\n";
        usersFile.close();
    }
    
    // Initialize other files
    ofstream menuFile(MENU_FILE, ios::app);
    ofstream ordersFile(ORDERS_FILE, ios::app);
    menuFile.close();
    ordersFile.close();
    
    initializeMenu();
    
    while (true) {
        cout << "\nRestaurant Management System\n";
        cout << "1. Admin\n";
        cout << "2. Customer\n";
        cout << "3. Exit\n";
        cout << "Choice: ";
        int choice;
        cin >> choice;
        if (choice == 3) break;
        if (choice != 1 && choice != 2) {
            cout << "Invalid choice.\n";
            continue;
        }
        string userType;
        User* userPtr = nullptr;
        if (login(userType, userPtr)) {
            if ((choice == 1 && userType == "admin") || (choice == 2 && userType == "customer")) {
                userPtr->showMenu();
            } else {
                cout << "Access denied.\n";
            }
            delete userPtr;
            userPtr = nullptr;
        }
    }
    return 0;
} 