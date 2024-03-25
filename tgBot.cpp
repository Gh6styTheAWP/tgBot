#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <tgbot/tgbot.h>
#include "sqlite3.h"
#include <map>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <functional>
#include <future>
#include <cstdio>
#include <chrono>
#include <thread>
using namespace std;
using namespace TgBot;
//проверка пробелов
int utf8_strlen(const string& str) {
    int length = 0;
    for (char c : str) {
        if ((c & 0xC0) != 0x80 && c != ' ') {  // Пропускаем пробелы
            length++;
        }
    }
    return length;
}
//active
void handleActiveCommand(Bot& bot, sqlite3* db, Message::Ptr message) {
    string sql = "SELECT username, COUNT(*) as message_count FROM messages GROUP BY username ORDER BY message_count DESC LIMIT 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int count = sqlite3_column_int(stmt, 1);
            bot.getApi().sendMessage(message->chat->id, u8"Самый активный пользователь: @" + username + u8" с " + to_string(count) + u8" сообщениями.");
        }
        else {
            bot.getApi().sendMessage(message->chat->id, u8"Сообщений нет.");
        }
    }
    else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}
//popular
void handlePopularCommand(Bot& bot, sqlite3* db, Message::Ptr message) {
    string sql = "SELECT message_text FROM messages";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        map<string, int> wordCount;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            istringstream iss(text);
            string word;
            while (iss >> word) {
                wordCount[word]++;
            }
        }

        string mostPopularWord;
        int maxCount = 0;
        for (const auto& pair : wordCount) {
            if (pair.second > maxCount) {
                mostPopularWord = pair.first;
                maxCount = pair.second;
            }
        }

        bot.getApi().sendMessage(message->chat->id, u8"Вот самое популярное слово на данный момент: " + mostPopularWord + u8". Оно было использовано " + to_string(maxCount) + u8" раз.");
    }
    else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}
//longer
void handleLongestCommand(Bot& bot, sqlite3* db, Message::Ptr message) {
    string sql = "SELECT username, message_text FROM messages ORDER BY LENGTH(message_text) DESC LIMIT 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* usernameColumn = sqlite3_column_text(stmt, 0);
            string username = usernameColumn ? reinterpret_cast<const char*>(usernameColumn) : "Unknown";

            const unsigned char* messageTextColumn = sqlite3_column_text(stmt, 1);
            string longestMessage = messageTextColumn ? reinterpret_cast<const char*>(messageTextColumn) : "";

            // Вычисляем длину сообщения
            int messageLength = utf8_strlen(longestMessage);

            // Формируем ответное сообщение
            string response = u8"Этот человек @" + username + u8" написал самое длинное сообщение на " + to_string(messageLength) + u8" символов. Вот оно:\n\n" + "\"" + longestMessage + "\"";
            bot.getApi().sendMessage(message->chat->id, response);
        }
        else {
            bot.getApi().sendMessage(message->chat->id, "Сообщений нет.");
        }
    }
    else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}
//reply
void handleReplyCommand(Bot& bot, sqlite3* db, Message::Ptr message) {
    string sql = "SELECT reply_to_message_id, chat_id, COUNT(*) as reply_count FROM messages WHERE reply_to_message_id IS NOT NULL GROUP BY reply_to_message_id ORDER BY reply_count DESC LIMIT 1";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        int stepResult = sqlite3_step(stmt);
        if (stepResult == SQLITE_ROW) {
            int64_t chatId = sqlite3_column_int64(stmt, 1);
            int replyToMessageId = sqlite3_column_int(stmt, 0);
            int replyCount = sqlite3_column_int(stmt, 2);

            bot.getApi().forwardMessage(message->chat->id, chatId, replyToMessageId);
            bot.getApi().sendMessage(message->chat->id, u8"На это сообщение ответили " + to_string(replyCount) + u8" раз.");
        }
        else if (stepResult == SQLITE_DONE) {
            bot.getApi().sendMessage(message->chat->id, "Сообщений нет.");
        }
        else {
            bot.getApi().sendMessage(message->chat->id, "An error occurred while processing the request.");
        }
    }
    else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        bot.getApi().sendMessage(message->chat->id, "Failed to process the request.");
    }
    sqlite3_finalize(stmt);
}
//clear
void handleClearCommand(Bot& bot, sqlite3* db, Message::Ptr message) {
    const char* deleteSql = "DELETE FROM messages;";
    char* errMessage = 0;
    int rc = sqlite3_exec(db, deleteSql, 0, 0, &errMessage);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMessage);
        sqlite3_free(errMessage);
        bot.getApi().sendMessage(message->chat->id, u8"Произошла ошибка при очистке базы данных.");
    }
    else {
        fprintf(stdout, "Database cleared successfully\n");
        bot.getApi().sendMessage(message->chat->id, u8"База данных успешно очищена.");
    }
}



int main() {
   
    //получаем токен из файла
    setlocale(LC_ALL, "Ru");
    ifstream file("C:\\Users\\saiin\\Desktop\\token.txt");
    string token;
    if (file.is_open()) {
        getline(file, token);
        file.close();
    }
    else {
        cerr << "Не удалось открыть файл с токеном" << endl;
        return 1;
    }
    Bot bot(token);

    //создание, обработка и очистка БД
    sqlite3* db;
    char* errMessage = 0;
    int rc = sqlite3_open("bot_database.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    }
    else {
        fprintf(stderr, "Opened database successfully\n");
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS messages ("  \
                  "id INTEGER PRIMARY KEY AUTOINCREMENT," \
                  "user_id INTEGER NOT NULL," \
                  "username TEXT," \
                  "message_text TEXT NOT NULL," \
                  "message_date DATETIME NOT NULL);";

    rc = sqlite3_exec(db, sql, 0, 0, &errMessage);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMessage);
        sqlite3_free(errMessage);
    }
    else {
        fprintf(stdout, "Table created successfully\n");
    }

    const char* deleteSql = "DELETE FROM messages;";
    rc = sqlite3_exec(db, deleteSql, 0, 0, &errMessage);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMessage);
        sqlite3_free(errMessage);
    }
    else {
        fprintf(stdout, "Messages cleared successfully\n");
    }
    const char* alterTableSql = "ALTER TABLE messages ADD COLUMN reply_to_message_id INTEGER;";
    sqlite3_exec(db, alterTableSql, 0, 0, &errMessage);

    const char* alterTableSql1 = "ALTER TABLE messages ADD COLUMN chat_id INTEGER;";
    sqlite3_exec(db, alterTableSql1, 0, 0, &errMessage);

    const char* alterTableSql2 = "ALTER TABLE messages ADD COLUMN message_id INTEGER;";
    sqlite3_exec(db, alterTableSql2, 0, 0, &errMessage);


    //обработчик help
    bot.getEvents().onCommand("help", [&bot](Message::Ptr message) {
        string response = u8"Привет! Меня зовут Nest Watcher, вот что я умею:\n1 - /act - Найду самого активного участника беседы и кол-во его сообщений.\n2 - /ppl - Найду наиболее популярное слово.\n3 - /lng - Найду самое длинное сообщение и его автора.\n4 - /rpl - Найду сообщение с наибольшим кол-вом ответов и его автора.\n(Функция в тестовом режиме, пожалуйста, не вызывайте ее, если нет сообщений с ответами. Иначе мне крышка.)";
        bot.getApi().sendMessage(message->chat->id, response);
        });
    //обработчик onAnyMessege
    bot.getEvents().onAnyMessage([&bot, &db](Message::Ptr message) {
        if (StringTools::startsWith(message->text, "/")) {
            return;
        }
        int replyToMessageId = message->replyToMessage ? message->replyToMessage->messageId : 0;
        string sql = "INSERT INTO messages (user_id, username, message_text, message_date, reply_to_message_id, chat_id, message_id) VALUES (?, ?, ?, datetime('now'), ?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
        sqlite3_bind_int(stmt, 1, message->from->id);
        sqlite3_bind_text(stmt, 2, message->from->username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, message->text.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, replyToMessageId);
        sqlite3_bind_int64(stmt, 5, message->chat->id);
        sqlite3_bind_int(stmt, 6, message->messageId);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            printf("Error inserting data: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
        });
    //обработчик active
    bot.getEvents().onCommand("act", [&bot, &db](Message::Ptr message) {
        handleActiveCommand(bot, db, message);
        });
    //обработчик popular
    bot.getEvents().onCommand("ppl", [&bot, &db](Message::Ptr message) {
        handlePopularCommand(bot, db, message);
        });
    //обработчик longer
    bot.getEvents().onCommand("lng", [&bot, &db](Message::Ptr message) {
        handleLongestCommand(bot, db, message);
        });
    //обработчик reply
    bot.getEvents().onCommand("rpl", [&bot, &db](Message::Ptr message) {
        handleReplyCommand(bot, db, message);
        });
    //обработчки clear
    bot.getEvents().onCommand("clear", [&bot, &db](Message::Ptr message) {
        handleClearCommand(bot, db, message);
        });

    try {
        printf("Bot started\n");
        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    }
    catch (TgException& e) {
        printf("error: %s\n", e.what());
    }
    sqlite3_close(db);
    return 0;
}
