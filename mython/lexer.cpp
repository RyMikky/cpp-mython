#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <iostream>
#include <cassert>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

    #define VALUED_OUTPUT(type) \
        if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

    #undef VALUED_OUTPUT

    #define UNVALUED_OUTPUT(type) \
        if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

    #undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }


    namespace detail {

        // проверка строки на то, что она является числом
        bool IsNumericRange(std::string_view str) {
            return (48 <= str[0] && str[0] <= 57) ? true : false;
        }

        // проверка строки на то, что она является числом
        bool IsNumericRange(char c) {
            return (48 <= c && c <= 57) ? true : false;
        }

        // проверка символа на то, что он является математическим
        bool IsMathematicSymbol(char c) {
            return __BASIC_MATHEMATIC_SYMBOLS__.count(c);
        }

        // проверка символа на соответствие базовым операндам
        bool IsFunctionalSymbol(char c) {
            return __BASIC_FUNCTIONALY_SYMBOLS__.count(c);
        }

    } // namespace detail

    Lexer::Lexer(std::istream& input) {
        BasicLinesReader(input);
        // деббаговая функция, включается по необходимости
        //BasicLinesPrintInCerr();
    }

    const Token& Lexer::CurrentToken() const {
        try
        {
            return _tokens_base.front();
        }
        catch (const std::exception&)
        {
            throw std::logic_error("Not implemented"s);
        }
    }

    Token Lexer::NextToken() {
        try
        {
            if (_tokens_base.size() > 0) {
                _tokens_base.pop_front();
            }

            if (_tokens_base.size() == 0) {
                return Token(token_type::Eof{});
            }
            else {
                return CurrentToken();
            }
        }
        catch (const std::exception&)
        {
            throw std::logic_error("Not implemented"s);
        }
    }

    // организатор экранирования
    bool Lexer::ShieldProtectionManager(char c) {
        if (_IsShilded) {
            switch (c)
            {
            case 't':
                _token += '\t';
                _IsShilded = false;
                return true;
            case 'n':
                _token += '\n';
                _IsShilded = false;
                return true;
            case '#':
                return false;
            default:
                _token += c;
                _IsShilded = false;
                return true;
            }
        }
        else {
            return false;
        }
    }
    // работа с символами если включены кавычки
    bool Lexer::QuotedProtectionManager(char c) {
        // если подняты двойные кавычки, и при этом получаем символ двойных кавычек
        if (_IsDoubleQuoteIsOpen && c == '\"') {
            return false;   // выходим с false, так как дальше символ поймает менеджер кавычек
        }
        // если подняты одинарные кавычки, и при этом получаем символ одинарных кавычек
        else if (_IsSingleQuoteIsOpen && c == '\'') {
            return false;   // выходим с false, так как дальше символ поймает менеджер кавычек
        }
        // если подняты какие-либо кавычки
        else if (_IsDoubleQuoteIsOpen || _IsSingleQuoteIsOpen ) {
            // проверяем на экранировку
            if (ShieldSymbolManager(c)) {
                return true;
            }
            else {
                // просто пишем символ в токен
                _token += c;
                return true;
            }
        }
        else {
            // передаем символ следующему менеджеру
            return false;
        }
    }
    // организатор включения экранировки
    bool Lexer::ShieldSymbolManager(char c) {
        if (c == '\\') {
            // устанавливаем флаг поднятия щита для следующего символа
            _IsShilded = true;
            return true;
        }
        else {
            return false;
        }
    }
    // организатор комментариев
    bool Lexer::CommentarSymbolManager(char c) {
        // символ должен быть "диезом"
        if (c == '#') {
            // если токен пуст, кавычки не начаты - признак того, что диез в самом начале
            if (_token.empty() && !_IsSingleQuoteIsOpen && !_IsDoubleQuoteIsOpen) {
                return true;   // прекращаем работу с текущей строкой, так как все будет закомментированно
            }
            // если токен Не пуст и кавычки не начаты
            else if (!_token.empty()) {
                AddStringToken(_token);             // анализиурем и записываем набранный токен
                _token.clear();                     // очищаем токен после обработки
                return true;                        // закрываем работу с текущей строкой, так как дальнейшее будет закоменнтированно
            }
            else {
                // при неопределенном поведении символа # выбрасываем ошибку
                std::cerr << "FILE::lexer.cpp"sv << std::endl;
                std::cerr << "FUNC::bool Lexer::CommentarSymbolManager(char c)"sv << std::endl;
                std::cerr << "INPUT_LINE::"sv << std::endl;
                std::cerr << "\t{ \""sv << _input_lines_history.back() << "\"}"sv << std::endl;
                std::cerr << "ERROR::Uncorrect \"#\" symbol parse"sv << std::endl;
                assert(false);
                return false;
            }
        }
        else {
            return false;
        }
    }
    // организатор комплексных символов
    bool Lexer::ComplexSymbolManager(char c) {
        if (c == '!' || c == '=' || c == '>' || c == '<') {
            // если кавычки не открыты
            if (!_IsSingleQuoteIsOpen && !_IsDoubleQuoteIsOpen) {
                // если это первое текущее вхождение возможного комплексного символа
                if (!_IsComplexSymbol) {
                    _token += c;                        // записываем символ в токен
                    _IsComplexSymbol = true;            // подымаем флаг ожидания продолжения (символ '=')
                    return true;
                }
                // если это вторичное подряд вхождение возможного комплексного символа
                else {
                    // если символ '='
                    if (c == '=') {
                        _token += c;                    // добавляем символ в токен
                        AddStringToken(_token);         // обрабатываем токен комплексного символа
                        _token.clear();                 // очищаем токен
                        _IsComplexSymbol = false;       // закрываем флаг комплексности
                        return true;
                    }
                    // если символ НЕ '='
                    else {
                        AddStringToken(_token);         // обрабатываем уже набраный токен
                        AddCharToken(c);             // обрабатываем полученный символ
                        _token.clear();                 // очищаем токен
                        _IsComplexSymbol = false;       // закрываем флаг комплексности
                        return true;
                    }
                }
            }
            // если кавычки открыты то просто записываем символ в токен
            else {
                _token += c;
                return true;
            }
        }
        else {
            return false;
        }
    }
    // организатор двойных кавычек
    bool Lexer::DoubleQuoteManager(char c){
        if (c == '\"') {
            // если не поднят ни один флаг начала строки в кавычках
            if (!_IsSingleQuoteIsOpen && !_IsDoubleQuoteIsOpen) {
                // подымаем флаг начала строки в двойных кавычках
                _IsDoubleQuoteIsOpen = true;
                return true;
            }
            // если флаг двойных кавычек уже поднят
            else if (_IsDoubleQuoteIsOpen) {
                _IsDoubleQuoteIsOpen = false;                                       // снимаем флаг строки в кавычках
                _tokens_base.push_back(Token(token_type::String{ _token }));   // обрабатываем полученный токен
                _token.clear();                                                     // очищаем токен
                return true;
            }
            // если открыта строка в одинарных кавычках
            else if (_IsSingleQuoteIsOpen) {
                _token += c;                          // просто добавляем текущие кавычки как обычный символ в строке
                return true;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    // организатор одинарных кавычек
    bool Lexer::SingleQuoteManager(char c){
        if (c == '\'') {
            // если не поднят ни один флаг начала строки в кавычках
            if (!_IsSingleQuoteIsOpen && !_IsDoubleQuoteIsOpen) {
                // подымаем флаг начала строки в одинарных кавычках
                _IsSingleQuoteIsOpen = true;
                return true;
            }
            // если флаг одинарных кавычек уже поднят
            else if (_IsSingleQuoteIsOpen) {
                _IsSingleQuoteIsOpen = false;                                       // снимаем флаг строки в кавычках
                _tokens_base.push_back(Token(token_type::String{ _token }));   // обрабатываем полученный токен
                _token.clear();                                                     // очищаем токен
                return true;
            }
            // если открыта строка в двойных кавычках
            else if (_IsDoubleQuoteIsOpen) {
                _token += c;                          // просто добавляем текущие кавычки как обычный символ в строке
                return true;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    // организатор символов пунктуации и форматирования
    bool Lexer::PunctuationSymbolManager(char c) {

        // символ должен быть пунктуационным и токен не пустой
        if ((c == ':' || c == ',' || c == '.' || c == '{' || c == '}') && !_token.empty()) {

            // если кавычки открыты
            if (_IsSingleQuoteIsOpen || _IsDoubleQuoteIsOpen) {
                // пишем символ как обычный печатный
                _token += c;
            }
            else {
                AddStringToken(_token);     // сначала записываем имеющийся токен
                AddCharToken(c);          // записываем токен математического символа
                _token.clear();             // очищаем токен для продолжения работы
            }

            return true;                    // запустит следующую итерацию цикла
        }
        else {
            return false;                   // передаст управление следующему менеджеру
        }
    }
    // организатор математических символов
    bool Lexer::MathematicSymbolManager(char c) {

        // символ должен быть математическим и токен не пустой
        if (detail::IsMathematicSymbol(c) && !_token.empty()) {

            // если кавычки открыты
            if (_IsSingleQuoteIsOpen || _IsDoubleQuoteIsOpen) {
                // пишем символ как обычный печатный
                _token += c;
            }
            else {
                AddStringToken(_token);     // сначала записываем имеющийся токен
                AddCharToken(c);          // записываем токен математического символа
                _token.clear();             // очищаем токен для продолжения работы
            }

            return true;                    // запустит следующую итерацию цикла
        }
        else {
            return false;                   // передаст управление следующему менеджеру
        }
    }

    // добавление токена типа Char
    void Lexer::AddCharToken(char с) {
        _tokens_base.push_back(Token(token_type::Char{ с }));
    }
    // анализатовать и добавить полученный токен из строки
    void Lexer::AddStringToken(std::string_view _token) {

        // ищем совпадения в константной мапе слов и выражений 
        if (__BASIC_LANGUAGE_IDENTIFICATORS__.count(_token)) {
            _tokens_base.push_back(__BASIC_LANGUAGE_IDENTIFICATORS__.at(_token));
        }

        // пытаемся записать токен как символ, если его не обработали менеджеры
        else if (__BASIC_FUNCTIONALY_SYMBOLS__.count(*_token.data())) {
            _tokens_base.push_back(Token(token_type::Char{ *_token.data() }));
        }

        // пытаемся записать токен как число
        else if (detail::IsNumericRange(_token)) {
            _tokens_base.push_back(Token(token_type::Number{ std::stoi(std::string(_token)) }));
        }

        else {
            // записываем токен как token_type::Id
            _tokens_base.push_back(Token(token_type::Id{ std::string(_token) }));
        }
    }

    // парсинг полученной входящей строки
    void Lexer::InputStringParser(std::string&& str) {

        // бежим по строке и заполняем токены 
        for (char c : str) {

            // если символ пробельный, токен НЕ пустой и не открыты какие либо кавычки
            if (c == ' ' && !_token.empty() && !_IsDoubleQuoteIsOpen && !_IsSingleQuoteIsOpen) {
                AddStringToken(_token);                              // анализируем и добавляем токен
                _token.clear();                                       // очищаем токен
                if (_IsComplexSymbol) _IsComplexSymbol = false;       // обнуляем флаг комплексного символа
            }

            // если символ пробельный, токен пустой и не открыты какие либо кавычки
            else if (c == ' ' && _token.empty() && !_IsSingleQuoteIsOpen && !_IsDoubleQuoteIsOpen) {
                continue; // пропускаем, так как отступами ведает вызывающая функция
            }

            // если активирован экран - передаем управление специализированному менеджеру
            else if (_IsShilded) {
                ShieldProtectionManager(c);
            }

            // если открыты кавычки - передаем управление специализированному менеджеру
            else if (QuotedProtectionManager(c)) {
                continue;
            }
            // если символ - один из спецсимволов или знаков пунктуации и разметки
            else if (detail::IsFunctionalSymbol(c)) 
            {
                // если символ "диез" признак комментария - передаем управление специализированному менеджеру
                if (CommentarSymbolManager(c)) {
                    break; // если менеджер вернул истину, то разбор строки прекратится
                }
                // если спецсимвол экран
                else if (ShieldSymbolManager(c)) {
                    continue;
                }
                // если спецсимвол это символ пунктуации и токен не пустой
                else if (PunctuationSymbolManager(c)) {
                    continue;
                }
                // если спецсимвол это математический оператор и токен не пустой
                else if (MathematicSymbolManager(c)) {
                    continue;
                }
                // если спецсимвол одинарная кавычка (апостроф)
                else if (SingleQuoteManager(c)) {
                    continue;
                }
                // если спецсимвол двойная кавычка
                else if (DoubleQuoteManager(c)) {
                    continue;
                }
                // если спецсимвол может являться началом комплексного символа
                else if (ComplexSymbolManager(c)) {
                    continue;
                }
                else {
                    // если ни один менеджер не принял символ
                    AddCharToken(c);      // записываем его как отдельный Char 
                    _token.clear();          // очищаем записи в токене и читаем дальше
                }
            }

            else {
                // ни одно условие не приняло символ, то просто записываем его в токен
                _token += c;
            }
        }

        // дописываем крайний оставшийся в строке токен, если он есть
        if (!_token.empty()) {
            AddStringToken(_token);
            _token.clear();
        }
    }

    // выставляет нужную табуляцию
    void Lexer::IndentManager(size_t factor) {
        for (size_t i = _indent_factor; i != factor; ++i) {
            _tokens_base.push_back(Token(token_type::Indent{}));
        }
        _indent_factor = factor;
    }
    // выставляет нужную детабуляцию
    void Lexer::DedentManager(size_t factor) {
        for (size_t i = factor; i != _indent_factor; ++i) {
            _tokens_base.push_back(Token(token_type::Dedent{}));
        }
        _indent_factor = factor;
    }

    // базовая функция получения строки
    void Lexer::BasicLinesReader(std::istream& input) {

        // флаг получения линии из потока
        bool IsLineBegin = false; 
        // текущий уровень табуляции
        size_t curren_indent_factor = 0;

        // циклически обрабатываем строки
        while (input)
        {
            std::string line; // получаем строку из потока
            std::getline(input, line);

            // чтобы из консоли выйти из цикла чтения и выполнить записанную команду
            if (line == "-e" || line == "-execute") {
                break;
            }

            // если строка получена, то ставим флаг
            if (!IsLineBegin && !line.empty()) {
                IsLineBegin = true;
            
                // для деббага сохраняем историю полученных линий
                _input_lines_history.push_back(line);

                // если строка начинается с пробельного символа, то возможно имеется табуляция
                if (line[0] == ' ') {
                    // сразу считаем кратно двум, так как два проблела образуют один уровень табуляции
                    curren_indent_factor = (line.find_first_not_of(' ') / 2);

                    // если уровень табуляции превышает базовый
                    if (curren_indent_factor > _indent_factor) {
                        // выставляем токен табуляции
                        IndentManager(curren_indent_factor);
                    }
                    // если уровень табуляции ниже базового
                    else if (curren_indent_factor < _indent_factor) {
                        // выставляем токен детабуляции
                        DedentManager(curren_indent_factor);
                    }
                }
                else {
                    // если строка не начинается с пробела, то проверяем закрытие табуляций
                    if (_indent_factor != 0) {
                        DedentManager(0);
                    }
                }

                // если строка начинается с "диеза" - пропускаем строку
                if (line[0] == '#') {
                    IsLineBegin = false;
                    continue;
                }

                // запускаем парсинг строки
                InputStringParser(std::move(line));

                // ставим токен перевода строки
                _tokens_base.push_back(Token(token_type::Newline{}));
                IsLineBegin = false;
            }
        }

        // проверяем закрытие табуляций перед закрытием строки
        if (_indent_factor != 0) {
            DedentManager(0);
        }

        // ставим последний токен конца документа 
        _tokens_base.push_back(Token(token_type::Eof{}));
    }
    // Функция для деббага - вывод полученной строки в std::cerr
    void Lexer::BasicLinesPrinter() {
        for (size_t i = 0; i != _input_lines_history.size(); ++i) {
            std::cerr << "Input line " << i << ": \"  " << _input_lines_history[i] << "  \"\n";
        }
    }

}  // namespace parse
