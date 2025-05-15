#include <tgbot/tgbot.h> // bot de teletram
#include <iostream> // entrada y salida por terminal
#include <curl/curl.h> // libcurl
#include <map> // añade map (como java.util.Map)
#include <vector> // similar a array list
#include <regex> // regex
#include <fstream> // permite trabajar con archivos
#include <sstream> // permite trabajar con datos en memoria
#include <nlohmann/json.hpp> // para trabajar con jsons
#include <cstdlib> // funciones generales
#include <thread> // para hilos

/**
 * Namespace X - importa todas las clases de X
 * Using U = A::B - asigna a x solamente A::B (no todas las clases de A)
 * */
using namespace std; // permite usar las funciones de std sin escribir std::count, std::vector...
using namespace TgBot; // igual pero con TgBot, para no poner cosas como TgBot::Bot
using json = nlohmann::json; // alias para poner json sin poner nlohmann::json

map<int64_t, map<string, string>> userSubscriptions; // mapa de mapas hehe asigna ID de telegram a subs y subs es ID de canal a nombre del canal
const string DB_FILE = "subs.json"; // archivo de backup de subs

// funcion de callback para libcurl para manejar la respuesta HTTP
/**
 * type* para punteros
 *
 * size_t = unsigned int; se usa para representar tamaños y se adapta a la arquitectura del PC
 * ---- size_t vale 4bytes en arq de 23 bits y 8 en arq de 64
 * ---- siempre positivo, por eso se usa para tamños (no existe un tamaño de memoria negativo... -1 giga XD)
 *
 * size = tamaño de bloque
 * nmemp = numero de bloques
 * */
CURLSTScode WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
    size_t newLength = size * nmemb; // tamaño de datos recibidos
    s->append((char*)contents, newLength); // convierte content en caracteres y los añade al final del puntero s
    return CURLSTS_OK; // esto es para que libcurl sepa que la funcion callback ha ido bien
}

// devuelve lo que devuelva la URL que le pasamos, como hacer un curl en la terminal
/**
 * type& es una referencia
 *
 *
 * */
string httpGet(const string& url) {
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (!curl) return "";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // .c_str() convierte un string a un puntero
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0"); // algunas webs bloquean si no hay user agent
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return readBuffer;
}

string getLatestVideoFromFeed(const string& channelId) {
    string feedUrl = "https://www.youtube.com/feeds/videos.xml?channel_id=" + channelId; // devuelve el "RSS" del youtuber en cuestion
    string xml = httpGet(feedUrl);

    smatch match;
    regex videoRegex("<link rel=\"alternate\" href=\"(https://www.youtube.com/watch\\?v=[^\"]+)\"/>"); // regex para recuperar el link de los videos

    if (regex_search(xml, match, videoRegex)) {
        return match[1]; // ultimo video
    } else {
        return "No se pudo encontrar el último vídeo.";
    }
}

void loadSubscriptions() {
    // intentamos leer el archivo de backup
    ifstream inFile(DB_FILE);
    if (!inFile.is_open()) return;

    json j;
    inFile >> j; // sobrecarga el contenido del archivo a un json, >> (overload operator)

    for (auto& [key, val] : j.items()) {
        int64_t userId = stoll(key); // stoll = string to long long
        for (auto& [alias, channelId] : val.items()) {
            userSubscriptions[userId][alias] = channelId.get<string>(); // cargamos el archivo en el mapa
        }
    }
}

void saveSubscriptions() {
    json j;
    for (const auto& [userId, channels] : userSubscriptions) {
        json userJson;
        for (const auto& [alias, channelId] : channels) {
            userJson[alias] = channelId;
        }
        j[to_string(userId)] = userJson;
    }

    ofstream outFile(DB_FILE);
    outFile << j.dump(4);
}

int main() {
    const char* token = std::getenv("TELEGRAM_BOT");

    if (token == nullptr) {
        std::cerr << "Error: El token no está definido." << std::endl;
        return 1;
    }

    cout << "Token recibido: " << token << endl;

    Bot bot(token);

    loadSubscriptions();

    // onCommand(name, lambda-function)
    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Hola! Usa /add <alias> <channel_id> para seguir canales.");
    });

    bot.getEvents().onCommand("add", [&bot](Message::Ptr message) {
        // -> arrow operator: se usa para acceder a clases y estructuras usando un puntero
        // es como cuando tienes una variable miCoche (de un objeto Coche) y haces miCoche.puertas
        // -> vs . (ambos existen en c) -> == acceso por puntero, . == acceso normal
        // curiosamente java se acerca mas a la idea de -> que .
        string text = message->text; // text es un atributo de message que contiene el texto que enviamos
        istringstream iss(text);
        string cmd, alias, channelId;
        iss >> cmd >> alias >> channelId;

        if (alias.empty() || channelId.empty()) {
            bot.getApi().sendMessage(message->chat->id, "Uso: /add <alias> <channel_id>");
            return;
        }

        userSubscriptions[message->chat->id][alias] = channelId;
        saveSubscriptions();
        bot.getApi().sendMessage(message->chat->id, "Canal añadido como \"" + alias + "\".");
    });

    bot.getEvents().onCommand("latest", [&bot](Message::Ptr message) {
        auto& channels = userSubscriptions[message->chat->id];
        if (channels.empty()) {
            bot.getApi().sendMessage(message->chat->id, "No tienes canales añadidos.");
            return;
        }

        for (const auto& [alias, channelId] : channels) {
            string videoUrl = getLatestVideoFromFeed(channelId);
            bot.getApi().sendMessage(message->chat->id, alias + ": " + videoUrl);
        }
    });

    bot.getEvents().onCommand("remove", [&bot](Message::Ptr message) {
        string text = message->text;
        istringstream iss(text);
        string cmd, alias;
        iss >> cmd >> alias;

        if (alias.empty()) {
            bot.getApi().sendMessage(message->chat->id, "Uso: /remove <alias>");
            return;
        }

        auto& channels = userSubscriptions[message->chat->id];
        if (channels.erase(alias)) {
            saveSubscriptions();
            bot.getApi().sendMessage(message->chat->id, "Canal \"" + alias + "\" eliminado.");
        } else {
            bot.getApi().sendMessage(message->chat->id, "Alias \"" + alias + "\" no encontrado.");
        }
    });

    bot.getEvents().onCommand("list", [&bot](Message::Ptr message) {
        int64_t userId = message->chat->id;
        const auto& channels = userSubscriptions[userId];

        if (channels.empty()) {
            bot.getApi().sendMessage(userId, "No tienes suscripciones guardadas.");
            return;
        }

        stringstream ss;
        ss << "Tus suscripciones:\n";
        for (const auto& [alias, channelId] : channels) {
            ss << "🔹 " << alias << " → " << channelId << "\n";
        }

        bot.getApi().sendMessage(userId, ss.str());
    });

    bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
        if (!message->text.empty()) {
            cout << "Mensaje recibido: " << message->text << endl;
        }
    });

    TgLongPoll longPoll(bot);

    try {
        while (true) {
            cout << "Long poll started..." << endl;
            longPoll.start();
            cout << "Long poll terminó, reiniciando en 1 segundo..." << endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const TgException& e) {
        cerr << "Error de TgBot: " << e.what() << endl;
        return 1;
    } catch (const exception& e) {
        cerr << "Error general: " << e.what() << endl;
        return 1;
    }

    return 0;
}
