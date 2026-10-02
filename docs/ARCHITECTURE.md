# Architecture

The program is a single translation unit (`restaurant_management_gui.cpp`) organised in layers:

```
Win32 windows (UI)  ->  Users / Auth  ->  Domain model  ->  Database (text files)
```

## 1. Class diagram

```mermaid
classDiagram
    direction LR

    class User {
        <<abstract>>
        #name_
        #email_
        #passHash_
        +role()*
        +openMainWindow(loginWindow)*
    }
    class Admin {
        +role()
        +openMainWindow(loginWindow)
    }
    class Customer {
        +role()
        +openMainWindow(loginWindow)
    }
    class Auth {
        +login(username, password)$
        +registerCustomer(username, password, email)$
    }
    User <|-- Admin
    User <|-- Customer
    Auth ..> User : creates

    class AppWindow {
        <<abstract>>
        #onCreate()*
        #onCommand(id, code)
        #onNotify(nmhdr)
        #onPageShown(index)
        #onDestroy()
        #selectPage(index)
        -drawButton(drawItem)
    }
    class Theme {
        <<static>>
        C_BG / C_BAND / C_ACCENT
        C_TEXT / C_HEAD / C_ROW_ALT
        +buttonColor(caption)$
        +shade(color, delta)$
    }
    AppWindow ..> Theme : paints with
    class LoginWindow
    class AdminWindow
    class CustomerWindow
    AppWindow <|-- LoginWindow
    AppWindow <|-- AdminWindow
    AppWindow <|-- CustomerWindow
    LoginWindow o-- User : current session
    AdminWindow --> User
    CustomerWindow --> User
    Admin ..> AdminWindow : opens
    Customer ..> CustomerWindow : opens

    class MenuItem {
        -id_
        -name_
        -category_
        -price_
        -stock_
        +toLine()
        +fromLine(line)$
    }
    class Order {
        -id_
        -customer_
        -type_
        -table_
        -status_
        -timestamp_
        -owner_
        -total_
        +addLine(name, qty, unit)
        +receiptText()
        +toLine()
        +fromLine(line)$
    }
    class OrderLine {
        <<struct>>
        name
        qty
        unit
        subtotal
    }
    class Reservation {
        -id_
        -customer_
        -table_
        -date_
        -time_
        -orderId_
        +toLine()
        +fromLine(line)$
    }
    Order *-- OrderLine
    Reservation ..> Order : optional link (orderId)

    class Database {
        <<static>>
        +loadMenu() / saveMenu()
        +loadOrders() / saveOrders()
        +loadReservations() / saveReservations()
        +loadUsers() / saveUsers()
        +tableFree(table, date, time)
        +writeReceipt(order)
        +init()
    }
    Database ..> MenuItem
    Database ..> Order
    Database ..> Reservation
    Auth ..> Database
    AdminWindow ..> Database
    CustomerWindow ..> Database
```

## 2. Window flow

```mermaid
flowchart TD
    A([WinMain]) --> B[Database::init<br/>create files + default data]
    B --> C[LoginWindow]
    C -->|Register| D[Auth::registerCustomer<br/>salted SHA-256 hash]
    D --> C
    C -->|Login| E{Auth::login}
    E -->|invalid| C
    E -->|admin| F[Admin::openMainWindow]
    E -->|customer| G[Customer::openMainWindow]
    F --> H[AdminWindow<br/>Menu / Orders / Reservations / Daily Sales]
    G --> I[CustomerWindow<br/>Order Food / Reserve a Table / My Orders]
    H -->|Logout| C
    I -->|Logout| C
    C -->|Exit / close| J([PostQuitMessage])
```

Logging out destroys the role window, which posts `WM_APP_LOGOUT` to the (hidden) login window; the
login window then shows itself again and drops the session.

Because the buttons are owner-drawn, Windows can no longer see which one is the default button, so
`LoginWindow` answers `DM_GETDEFID` with the Login button; this keeps **Enter = Login** working.

## 3. Order lifecycle

```mermaid
stateDiagram-v2
    [*] --> Pending : customer places order<br/>(receipt written, dine-in table reserved)
    Pending --> Accepted : admin accepts<br/>(stock checked, then deducted)
    Pending --> Rejected : admin rejects<br/>(dine-in table released)
    Accepted --> [*] : counted in daily sales
    Rejected --> [*]
```

## 4. Data storage

```mermaid
flowchart LR
    DB[(Database class)]
    DB --- U[users.txt<br/>username, salt$hash, role, email]
    DB --- M[menu.txt<br/>id, name, category, price, stock]
    DB --- O[orders.txt<br/>id, customer, ..., items, owner]
    DB --- R[reservations.txt<br/>id, customer, table, date, time, orderId]
    DB --- RC[receipts/receipt_N.txt]
```

Every `save*` call rewrites the whole file and every `load*` call re-reads it, so each window always works
on fresh data and no state is cached between actions.

## 5. Design notes

- **Polymorphism drives navigation:** `LoginWindow` never checks the role. It calls
  `user->openMainWindow()` and the dynamic type (`Admin` or `Customer`) decides which window opens.
- **One window base class:** `AppWindow` owns the Win32 plumbing (window procedure, control factory,
  page switching). Derived windows only override hooks such as `onCreate` and `onCommand`.
- **Pages, not tabs:** each screen section is a group of child controls shown or hidden by `selectPage()`,
  driven by owner-drawn navigation buttons in the top bar; the open page's button is drawn in the accent colour.
- **Theme lives in the base class:** colours, button drawing and list striping are handled once in `AppWindow`
  (see section 6), so `LoginWindow`, `AdminWindow` and `CustomerWindow` are themed without extra code.

## 6. Colour theme and rendering

`AppWindow::handle()` intercepts a few standard messages to paint the interface:

```mermaid
flowchart LR
    W[Window message] --> E[WM_ERASEBKGND]
    W --> S[WM_CTLCOLORSTATIC]
    W --> D[WM_DRAWITEM]
    W --> N[WM_NOTIFY / NM_CUSTOMDRAW]
    E --> E1[cream background + dark top bar + orange line]
    S --> S1[transparent labels: white in the bar, orange headings, brown body text]
    D --> D1[drawButton: rounded rectangle, colour from caption, darker when pressed, grey when disabled]
    N --> N1[ListView rows: white / peach stripes, selection left to Windows]
```

- **Top bar:** the first `BAND_H` (60) pixels of every window. Labels whose rectangle lies inside it are drawn in white.
- **Buttons:** created with `BS_OWNERDRAW`. Navigation buttons use the accent colour when selected
  (`shown` page index) and a muted brown otherwise; other buttons call `buttonColor(caption)`
  (red / green / blue / grey / orange).
- **Tables:** `list()` sets white row colours and text colour; `NM_CUSTOMDRAW` alternates row backgrounds.
- **Re-skinning:** only the `C_*` constants and `buttonColor()` need to change.
- **Persistence is isolated:** only `Database` touches the files; models know their own line format
  (`toLine` / `fromLine`) but never open files.
