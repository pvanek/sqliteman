-- SPDX-FileCopyrightText: 2026 Petr Vanek
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- The sample database the manual's screenshots show: a small shop.

CREATE TABLE Customer (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE,
    city TEXT,
    joined TEXT DEFAULT (date('now'))
);

CREATE TABLE Product (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    price REAL NOT NULL CHECK (price >= 0),
    details TEXT, -- JSON
    picture BLOB
);

CREATE TABLE "Order" (
    id INTEGER PRIMARY KEY,
    customer INTEGER NOT NULL REFERENCES Customer (id) ON DELETE CASCADE,
    placed TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL DEFAULT 'new' CHECK (status IN ('new', 'paid', 'shipped'))
);

CREATE TABLE OrderItem (
    "order" INTEGER NOT NULL REFERENCES "Order" (id) ON DELETE CASCADE,
    product INTEGER NOT NULL REFERENCES Product (id),
    quantity INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY ("order", product)
) WITHOUT ROWID;

CREATE TABLE AuditLog (
    at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    message TEXT
);

CREATE INDEX order_customer ON "Order" (customer);
CREATE INDEX customer_city ON Customer (city COLLATE NOCASE);

CREATE VIEW OrderTotal AS
SELECT o.id, c.name AS customer, sum(i.quantity * p.price) AS total
FROM "Order" o
JOIN Customer c ON c.id = o.customer
JOIN OrderItem i ON i."order" = o.id
JOIN Product p ON p.id = i.product
GROUP BY o.id;

CREATE TRIGGER order_paid AFTER UPDATE OF status ON "Order"
WHEN NEW.status = 'paid'
BEGIN
    INSERT INTO AuditLog (message) VALUES ('Order ' || NEW.id || ' paid');
END;

INSERT INTO Customer (name, email, city, joined) VALUES
    ('Alice Novák', 'alice@example.com', 'Praha', '2025-03-14'),
    ('Bob Dvořák', 'bob@example.com', 'Brno', '2025-06-02'),
    ('Cecilie Svobodová', NULL, 'Praha', '2025-11-20'),
    ('David Černý', 'david@example.com', 'Ostrava', '2026-01-08'),
    ('Eva Procházková', 'eva@example.com', NULL, '2026-04-30');

INSERT INTO Product (id, name, price, details) VALUES
    (1, 'Teapot', 24.9, '{"colour": "white", "volume": 1.2, "tags": ["ceramic", "kitchen"]}'),
    (2, 'Cup', 6.5, '{"colour": "blue", "volume": 0.3}'),
    (3, 'Green tea, 100 g', 9.8, NULL),
    (4, 'Strainer', 3.2, '{"material": "steel"}');

INSERT INTO "Order" (id, customer, placed, status) VALUES
    (1, 1, '2026-09-01 10:12:00', 'shipped'),
    (2, 2, '2026-09-15 16:40:00', 'paid'),
    (3, 1, '2026-10-02 08:05:00', 'new'),
    (4, 4, '2026-10-20 19:30:00', 'new');

INSERT INTO OrderItem ("order", product, quantity) VALUES
    (1, 1, 1), (1, 2, 4), (2, 3, 2), (2, 4, 1), (3, 2, 2), (4, 1, 1), (4, 3, 5);
