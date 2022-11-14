#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>
#include <deque>
#include <map>
#include <set>

using namespace std::literals;

const std::set<char> __BASIC_FUNCTIONALY_SYMBOLS__ =
    { ',' /* запятая */, '.' /* точка */, '!' /* воскл.знак */, '?' /* вопр.знак */, ':' /* двоеточие */, ';' /* тчк.запятой */
    , '<' /* лев.стрелка */, '>' /* пр.стрелка */, '=' /* равно */, '"' /* кавычка */, '\'' /* апостров */, '\\' /* обр.слеш */
    , '/' /* прям.слеш */, '\t' /* табуляция */, '\n' /* перевод строки */, '+' /* плюс */, '-' /* минус */, '*' /* умножить */
    , '%' /* взятие остатка */, '^' /* галка */, '(' /* отк.скобка */, ')' /* зак.скобка */, '#' /* диез =^_^= */
    , '{' /* отк.фиг.скобка */, '}' /* зак.фиг.скобка */, '\\' /* экран */ };

const std::set<char> __BASIC_MATHEMATIC_SYMBOLS__ =
    { '+' /* плюс */, '-' /* минус */, '*' /* умножить */, '/' /* разделить */
    , '%' /* взятие остатка */, '(' /* отк.скобка */, ')' /* зак.скобка */ };

namespace parse {

    namespace token_type {

        struct Number {  // Лексема «число»
            int value;   // число
        };

        struct Id {             // Лексема «идентификатор»
            std::string value;  // Имя идентификатора
        };

        struct Char {    // Лексема «символ»
            char value;  // код символа
        };

        struct String {  // Лексема «строковая константа»
            std::string value;
        };

        struct Class {};    // Лексема «class»
        struct Return {};   // Лексема «return»
        struct If {};       // Лексема «if»
        struct Else {};     // Лексема «else»
        struct Def {};      // Лексема «def»
        struct Newline {};  // Лексема «конец строки»
        struct Print {};    // Лексема «print»
        struct Indent {};   // Лексема «увеличение отступа», соответствует двум пробелам
        struct Dedent {};   // Лексема «уменьшение отступа»
        struct Eof {};      // Лексема «конец файла»
        struct And {};      // Лексема «and»
        struct Or {};       // Лексема «or»
        struct Not {};      // Лексема «not»
        struct Eq {};       // Лексема «==»
        struct NotEq {};    // Лексема «!=»
        struct LessOrEq {};     // Лексема «<=»
        struct GreaterOrEq {};  // Лексема «>=»
        struct None {};         // Лексема «None»
        struct True {};         // Лексема «True»
        struct False {};        // Лексема «False»

    }  // namespace token_type

    using TokenBase
        = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                       token_type::Class, token_type::Return, token_type::If, token_type::Else,
                       token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                       token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                       token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                       token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    const std::map<std::string_view, Token> __BASIC_LANGUAGE_IDENTIFICATORS__ = {
        { "class"sv, Token(token_type::Class{})}, { "return"sv, Token(token_type::Return{})}, { "print"sv, Token(token_type::Print{})}
        , { "def"sv, Token(token_type::Def{})}, { "None"sv, Token(token_type::None{})}, { "\n"sv, Token(token_type::Newline{})}
        , { "if"sv, Token(token_type::If{})}, { "else"sv, Token(token_type::Else{})}
        , { "and"sv, Token(token_type::And{})}, { "or"sv, Token(token_type::Or{})}, { "not"sv, Token(token_type::Not{})}
        , { "&&"sv, Token(token_type::And{})}, { "||"sv, Token(token_type::Or{})}
        , { "eq"sv, Token(token_type::Eq{})}, { "=="sv, Token(token_type::Eq{})}
        , { "NotEq"sv, Token(token_type::NotEq{})}, { "!="sv, Token(token_type::NotEq{})}
        , { "LessOrEq"sv, Token(token_type::LessOrEq{})}, { "GreaterOrEq"sv, Token(token_type::GreaterOrEq{})}
        , { "<="sv, Token(token_type::LessOrEq{})}, { ">="sv, Token(token_type::GreaterOrEq{})}
        , { "True"sv, Token(token_type::True{})}, { "False"sv, Token(token_type::False{})}
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    namespace detail {

        // проверка строки на то, что она является числом
        bool IsNumericRange(std::string_view str);

        // проверка символа на то, что он является числом
        bool IsNumericRange(char c);

        // проверка символа на то, что он является математическим
        bool IsMathematicSymbol(char c);

        // проверка символа на соответствие базовым операндам
        bool IsFunctionalSymbol(char c);

    } // namespace detail

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
        [[nodiscard]] const Token& CurrentToken() const;

        // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
        Token NextToken();

        // Если текущий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& Expect() const;

        // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void Expect(const U& /*value*/) const;

        // Если следующий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& ExpectNext();

        // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void ExpectNext(const U& /*value*/);

    private:

        // ----------------------------- булевые флаги парсинга строки ------------------------------------------------

        bool _IsDoubleQuoteIsOpen = false;                            // флаг открытия строки в кавычках "
        bool _IsSingleQuoteIsOpen = false;                            // флаг открытия строки апострофом '
        bool _IsComplexSymbol = false;                                // флаг возможного комплексного символа, например >= или !=
        bool _IsShilded = false;                                      // флаг щита поднимается после обратного слеша '\'

        // ----------------------------- базовые поля класса ----------------------------------------------------------
        
        // так как в строке может быть много различных идентификаций и определений то
        std::string _token;                                           // переменная набора токена
        size_t _current_token_index = 0;                              // номер текущего токена
        std::deque<Token> _tokens_base;                               // база полученных токенов
        size_t _indent_factor = 0;                                    // уровень табуляции, проверяется для каждой строки

        std::deque<std::string> _input_lines_history;                 // история входящих линий

        // ----------------------------- внутренние методы работы с символами -----------------------------------------

        bool ShieldProtectionManager(char c);                         // работа с символами если включен экран
        bool QuotedProtectionManager(char c);                         // работа с символами если включены кавычки
        bool ShieldSymbolManager(char c);                             // организатор включения экранировки
        bool CommentarSymbolManager(char c);                          // организатор комментариев
        bool ComplexSymbolManager(char c);                            // организатор комплексных символов
        bool DoubleQuoteManager(char c);                              // организатор двойных кавычек
        bool SingleQuoteManager(char c);                              // организатор одинарных кавычек
        bool PunctuationSymbolManager(char c);                        // организатор символов пунктуации и форматирования
        bool MathematicSymbolManager(char c);                         // организатор математических символов

        // ----------------------------- внутренние методы загрузки токенов -------------------------------------------

        void AddCharToken(char с);                                    // добавление токена типа Char
        void AddStringToken(std::string_view token);                  // анализатовать и добавить полученный токен из строки

        // ----------------------------- внутренние методы парсинга ---------------------------------------------------

        void InputStringParser(std::string&& str);                    // парсинг полученной входящей строки

        void IndentManager(size_t factor);                            // выставляет нужную табуляцию
        void DedentManager(size_t factor);                            // выставляет нужную детабуляцию

        void BasicLinesReader(std::istream& input);                   // базовая функция получения строки
        void BasicLinesPrinter();                                     // Функция для деббага - вывод полученной строки в std::cerr
    };


    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Lexer::Expect() const {
        using namespace std::literals;
        try
        {
            if (CurrentToken().Is<T>()) {
                return *(CurrentToken().TryAs<T>());
            }
            throw LexerError("Not implemented"s);
        }
        catch (const std::exception&)
        {
            throw LexerError("Not implemented"s);
        }
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Lexer::Expect(const U& value) const {
        using namespace std::literals;
        try
        {
            if (!CurrentToken().Is<T>()) {
                throw LexerError("Not implemented"s);
            }
            
            T _token = *(CurrentToken().TryAs<T>());

            if constexpr (std::is_same<T, token_type::Number>::value) {
                if (value != _token.value) {
                    throw LexerError("Not implemented"s);
                }
            }

            if constexpr (std::is_same<T, token_type::Id>::value) {
                if (value != _token.value) {
                    throw LexerError("Not implemented"s);
                }
            }

            if constexpr (std::is_same<T, token_type::String>::value) {
                if (value != _token.value) {
                    throw LexerError("Not implemented"s);
                }
            }

            if constexpr (std::is_same<T, token_type::Char>::value) {
                if (value != _token.value) {
                    throw LexerError("Not implemented"s);
                }
            }
        }
        catch (const std::exception&)
        {
            throw LexerError("Not implemented"s);
        }
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Lexer::ExpectNext() {
        using namespace std::literals;
        _tokens_base.pop_front();
        return this->Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Lexer::ExpectNext(const U& value) {
        using namespace std::literals;
        _tokens_base.pop_front();
        this->Expect<T>(value);
    }

}  // namespace parse