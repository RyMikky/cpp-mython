#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string __ADD_METHOD__ = "__add__"s;
        const string __INIT_METHOD__ = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        // вычисления сразуже вносим в таблицу символов согласно имени переменной
        closure[_var] = _rv.get()->Execute(closure, context);
        // возвращаем уже из таблицы символов
        return closure.at(_var);
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) 
        : _var(var), _rv(std::move(rv)) {
    }

    VariableValue::VariableValue(const std::string& var_name) {
        _dotted_ids.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) 
        : _dotted_ids(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {

        if (closure.count(_dotted_ids[0])) {
            // сюда будем перезаписывать таблицу символов
            Closure inner_closure = closure; 
            // возвращаемое значение
            ObjectHolder result = inner_closure.at(_dotted_ids[0]);
            // бежим по циклу начиная со второго элемента
            for (size_t i = 1; i != _dotted_ids.size(); ++i) {
                // проверяем наличие нужного поля в таблице символов
                if (inner_closure.count(_dotted_ids[i - 1])) {
                    // приводим первый элемент к объекту класса
                    runtime::ClassInstance* item = inner_closure.at(_dotted_ids[i - 1]).TryAs<runtime::ClassInstance>();
                    // перезаписываем таблицу символов
                    inner_closure = item->Fields();
                    // если в таблице символовм полученного элемента есть нужное нам поле
                    if (inner_closure.count(_dotted_ids[i])) {
                        // перезаписываем результат
                        result = inner_closure.at(_dotted_ids[i]);
                    }
                    else {
                        throw std::runtime_error("here is not a variable whit current name");
                    }
                }
                else {
                    throw std::runtime_error("here is not a variable whit current name");
                }
            }
            return result;
        }
        else {
            throw std::runtime_error("here is not a variable whit current name");
        }
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<ast::Print>(make_unique<VariableValue>(name));
    }

    Print::Print(unique_ptr<Statement> argument) {
        _args.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) 
        : _args(std::move(args)){
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        // берем поток вывода куда будем отправлять данные
        std::ostream& out = context.GetOutputStream();
        // флаг первого элемента для выставления пробела
        bool IsFirst = true;
        // перебираем аргументы
        for (auto& arg : _args) {
            // подготавливаем элемент к печати
            runtime::ObjectHolder item = arg->Execute(closure, context);

            if (IsFirst) {
                // если в элементе есть что печатать
                if (item) {
                    item.Get()->Print(out, context);
                }
                else {
                    // если в элементе нечего печатать
                    out << "None";
                }
                IsFirst = false;
            }
            else {
                if (item) {
                    out << ' ';
                    item.Get()->Print(out, context);
                }
                else {
                    out << " None";
                }
            }
        }
        out << '\n';

        return ObjectHolder::None();
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args) 
        : _object(std::move(object)), _method(std::move(method)), _args(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        // подготавливаем объект
        runtime::ClassInstance* obj = _object->Execute(closure, context).TryAs<runtime::ClassInstance>();
        // ищем требуемый метод
        if (obj->HasMethod(_method, _args.size())) {
            // подготавливаем аргументы для вызова
            std::vector<ObjectHolder> obj_args;
            for (auto& arg : _args) {
                obj_args.push_back(arg->Execute(closure, context));
            }
            // возвращаем результат вызова метода
            return obj->Call(_method, obj_args, context);
        }
        return ObjectHolder::None();
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        // выполняем основную инструкию из Stringify
        runtime::ObjectHolder result = _argument->Execute(closure, context);
        // смотрим что получилось после Execute
        if (!result) {
            return ObjectHolder::Own(runtime::String("None"));
        }
        else {
            // загружаем представление в поток
            std::ostringstream str;
            result->Print(str, context);
            // на основе полученной строки создаём runtime::ObjectHolder
            return ObjectHolder::Own(runtime::String(str.str()));
        }
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        
        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);
        
        // пытаемся оба преобразовать в число
        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
            // получаем результат сложения чисел
            int add_result = lhs.TryAs<runtime::Number>()->GetValue() + rhs.TryAs<runtime::Number>()->GetValue();
            // возвращаем runtime::ObjectHolder с результатом вычисления
            return ObjectHolder().Own(std::move(runtime::Number(add_result)));
        }
        // пытаемся оба преобразовать в строку
        else if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
            // получаем результат сложения строк
            std::string add_result = lhs.TryAs<runtime::String>()->GetValue() + rhs.TryAs<runtime::String>()->GetValue();
            // возвращаем runtime::ObjectHolder с результатом вычисления
            return ObjectHolder().Own(std::move(runtime::String(add_result)));
        }
        // пытаемся левое привести к объекту класса, а правое к объекту
        else if (lhs.TryAs<runtime::ClassInstance>() && rhs.TryAs<runtime::Object>()) {
            runtime::ClassInstance* lhs_class = lhs.TryAs<runtime::ClassInstance>();

            // првоеряем наличие метода сложения в классе
            if (lhs_class->HasMethod(__ADD_METHOD__, 1)) {
                return lhs_class->Call(__ADD_METHOD__, { rhs }, context);
            }
            else {
                throw std::runtime_error("lhs and rhs arguments can't be added");
            }
        }
        else {
            throw std::runtime_error("lhs and rhs arguments can't be added");
        }
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {

        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);

        // пытаемся оба преобразовать в число
        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
            // получаем результат вычитания чисел
            int add_result = lhs.TryAs<runtime::Number>()->GetValue() - rhs.TryAs<runtime::Number>()->GetValue();
            // возвращаем runtime::ObjectHolder с результатом вычисления
            return ObjectHolder().Own(std::move(runtime::Number(add_result)));
        }
        else {
            throw std::runtime_error("lhs and rhs arguments cant be added");
        }
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {

        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);

        // пытаемся оба преобразовать в число
        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
            // получаем результат умножения чисел
            int add_result = lhs.TryAs<runtime::Number>()->GetValue() * rhs.TryAs<runtime::Number>()->GetValue();
            // возвращаем runtime::ObjectHolder с результатом вычисления
            return ObjectHolder().Own(std::move(runtime::Number(add_result)));
        }
        else {
            throw std::runtime_error("lhs and rhs arguments cant be added");
        }
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {

        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);

        // пытаемся оба преобразовать в число
        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {

            // смотрим не равно ли нулю
            if (rhs.TryAs<runtime::Number>()->GetValue() != 0) {
                // получаем результат деления чисел
                int add_result = lhs.TryAs<runtime::Number>()->GetValue() / rhs.TryAs<runtime::Number>()->GetValue();
                // возвращаем runtime::ObjectHolder с результатом вычисления
                return ObjectHolder().Own(std::move(runtime::Number(add_result)));
            }
            else {
                throw std::runtime_error("lhs and rhs arguments cant be added");
            }
        }
        else {
            throw std::runtime_error("lhs and rhs arguments cant be added");
        }
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& сontext) {
        
        // последовательно выполняем инструкции
        for (auto& arg : _args) {
            try
            {
                // если инструкция выполняет условие if/elst или вызов метода
                if (dynamic_cast<IfElse*>(arg.get()) || dynamic_cast<MethodCall*>(arg.get()))
                {
                    // получаем результат вызова
                    runtime::ObjectHolder result = arg->Execute(closure, сontext);

                    if (result) {
                        // если результат получен то возвращаем его в ретурн
                        return result;
                    }
                }

                // в противном случае просто выполняем инструкцию
                // например инструкции инициализации класса, объекта, поля и т.п.
                else {
                    arg->Execute(closure, сontext);
                }

            }
            catch (const ast::return_data_exp& result)
            {
                // ловим кетч от ретурна и останавливаем выполнение команды
                return ObjectHolder::Own(runtime::String(result.what()));
                break;
            }
        }
        return ObjectHolder().None();
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {

        // подготавливаем результат вычислений того что в ретурне написано
        runtime::ObjectHolder pre_result = _stmt->Execute(closure, context);
        std::stringstream stream;

        // выводим результат вычислений ретурна в строку
        pre_result->Print(stream, context);
        std::string to_string_result = stream.str();

        // бросаем исключение для дальнейшего отлова в Compound::Execute
        throw ast::return_data_exp(std::move(to_string_result));
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) 
        : _cls(std::move(cls)) {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        closure[_cls.TryAs<runtime::Class>()->GetName()] = _cls;
        return runtime::ObjectHolder::None();
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv) : _object(std::move(object)), _field_name(std::move(field_name)), _rv(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, [[maybe_unused]] Context& context) {

        // приводимся к экземпляру класса
        runtime::ClassInstance* item = _object.Execute(closure, context).TryAs<runtime::ClassInstance>();
        // присваиваем или создаем новое поле из _field_name
        item->Fields()[_field_name] = _rv->Execute(closure, context);
        // возвращаем уже из таблицы экземпляра
        return item->Fields().at(_field_name);
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body) 
        : _condition(std::move(condition))
        , _if_body(std::move(if_body))
        , _else_body(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (_condition->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
            return _if_body->Execute(closure, context);
        }
        else {
            if (_else_body) {
                return _else_body->Execute(closure, context);
            }
            else {
                return ObjectHolder::None();
            }
        }
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);

        // пытаемся оба преобразовать в булеан
        if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {

            // если левое True
            if (lhs.TryAs<runtime::Bool>()->GetValue()) {

                return ObjectHolder().Own(std::move(runtime::Bool(true)));
            }
            // если левое False, но правое True
            else if (rhs.TryAs<runtime::Bool>()->GetValue()) {
                return ObjectHolder().Own(std::move(runtime::Bool(true)));
            }
            else {
                // оба False
                return ObjectHolder().Own(std::move(runtime::Bool(false)));
            }
        }
        else {
            throw std::runtime_error("unknow lhs and rhs arguments");
        }
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        
        // выполняем левое и правое выражение
        runtime::ObjectHolder lhs = _lhs->Execute(closure, context);
        runtime::ObjectHolder rhs = _rhs->Execute(closure, context);

        // пытаемся оба преобразовать в булеан
        if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {

            // если левое и првое True
            if (lhs.TryAs<runtime::Bool>()->GetValue() && rhs.TryAs<runtime::Bool>()->GetValue()) {

                return ObjectHolder().Own(std::move(runtime::Bool(true)));
            }

            else {
                return ObjectHolder().Own(std::move(runtime::Bool(false)));
            }
        }
        else {
            throw std::runtime_error("unknow lhs and rhs arguments");
        }
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        
        // выполняем выражение
        runtime::ObjectHolder arg = _argument->Execute(closure, context);
        
        // пытаемся преобразовать в булеан
        if (arg.TryAs<runtime::Bool>()) {

            // если полученное значение True
            if (arg.TryAs<runtime::Bool>()->GetValue()) {
                // инвертируем в False
                return ObjectHolder().Own(std::move(runtime::Bool(false)));
            }
            else {
                return ObjectHolder().Own(std::move(runtime::Bool(true)));
            }
        }
        else {
            throw std::runtime_error("unknow argument");
        }
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs)), _cmp(cmp) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        bool result = _cmp(_lhs->Execute(closure, context), _rhs->Execute(closure, context), context);
        return ObjectHolder().Own<runtime::Bool>(std::move(result));
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) 
        : _class(class_), _args(std::move(args)) {
    }

    NewInstance::NewInstance(const runtime::Class& class_) 
        : _class(class_){
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {

        // создаём объект экземпляра класса в куче с помощью ObjectHolder::Own
        runtime::ObjectHolder new_instance = ObjectHolder::Own(runtime::ClassInstance(_class));
        // кастуем приведение к экземпляру для последующей инициализации полей
        runtime::ClassInstance* inst = new_instance.TryAs<runtime::ClassInstance>();
        
        // если есть метод инициализации и количество аргументов совпадает
        if (inst->HasMethod(__INIT_METHOD__, _args.size())) {
            // подготавливаем вектор аргументов
            std::vector<ObjectHolder> actual_args;
            for (const auto& arg : _args) {
                actual_args.push_back(arg->Execute(closure, context));
            }
            // отправляем в функцию инициализации полей
            inst->Call(__INIT_METHOD__, actual_args, context);
        }
        
        return new_instance;         // возвращаем созданный объект
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
        : _body(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        return _body->Execute(closure, context);
    }

}  // namespace ast