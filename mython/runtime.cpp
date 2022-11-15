#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <iostream>

using namespace std;

namespace runtime {

    namespace {
        const string __PRINT_METHOD__ = "__str__"s;
        const string __EQUAL_METHOD__ = "__eq__"s;
        const string __LESS_METHOD__ = "__lt__"s;
    }  // namespace

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {

        runtime::Object* data = object.Get();
        if (!data) {
            return false;
        }
        else {

            if (object.TryAs<Bool>()) {
                return object.TryAs<Bool>()->GetValue();
            }
            else if (object.TryAs<Number>()) {
                return object.TryAs<Number>()->GetValue() == 0 ? false : true;
            }
            else if (object.TryAs<String>()) {
                return object.TryAs<String>()->GetValue().empty() ? false : true;
            }
            else {
                return false;
            }
        }
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {

        // если у класса есть метод "__str__"
        if (HasMethod(__PRINT_METHOD__, 0)) {

            // получаем результат вызова функции
            runtime::ObjectHolder call_back = Call(__PRINT_METHOD__, {}, context);

            // пытаемся привести результат к строке
            if (call_back.TryAs<runtime::String>()) {
                call_back.TryAs<runtime::String>()->Print(os, context);
            }
            // пытаемся привести результат к числу
            else if (call_back.TryAs<runtime::Number>()) {
                call_back.TryAs<runtime::Number>()->Print(os, context);
            }
            // пытаемся привести результат к булеану
            else if (call_back.TryAs<runtime::Bool>()) {
                call_back.TryAs<runtime::Bool>()->Print(os, context);
            }
        }
        else {
            // выводим в поток адрес объекта в памяти
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        // пытаемся найти нужный метод
        const runtime::Method* founded = _base_class.GetMethod(method);
        // если метод найден и количество параметров совпадает с указанным
        if (founded && founded->formal_params.size() == argument_count) {
            return true;
        }
        else {
            return false;
        }
    }

    Closure& ClassInstance::Fields() {
        return _instance_closure;
    }

    const Closure& ClassInstance::Fields() const {
        return _instance_closure;
    }

    ClassInstance::ClassInstance(const Class& cls) 
        : _base_class(cls) {
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args, Context& context) {
        if (HasMethod(method, actual_args.size())) {

            // берем нужный нам метод
            const runtime::Method* _method = _base_class.GetMethod(method);
            // для его выполнения создаём таблицу символов выполнения
            Closure _executable_closure;
            // заполняем созданную таблицу символов по переданным аргументам
            for (size_t i = 0; i != actual_args.size(); ++i) {
                _executable_closure[_method->formal_params[i]] = actual_args[i];
            }

            // добавляем в таблицу крайнее поле о вызывающем классе
            _executable_closure["self"] = ObjectHolder::Share(*this);

            // производим выполнение метода
            return _method->body->Execute(_executable_closure, context);
        }
        else {
            // Если метод не найден выбрасываем исключение
            throw std::runtime_error("Method \""s + method + "\" is not found"s);
        }
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : _class_name(name), _class_methods(std::move(methods)), _class_parent(parent) {
    }

    const Method* Class::GetMethod(const std::string& name) const {
        // ищем сначала в списке методов текущего класса
        for (auto& item : _class_methods) {
            if (item.name == name) {
                return &item;
            }
        }
        // если не нашли ищем в классе-родителе, если он имеется
        if (_class_parent) {
            return _class_parent->GetMethod(name);
        }
        return nullptr;
    }

    const std::string& Class::GetName() const {
        return _class_name;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class "sv << _class_name;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& сontext) {

        // если оба вернули nullptr, то это по сути означает что там объекты типа None
        if (!lhs.Get() && !rhs.Get()) {
            return true;
        }
        else {
            // пытаемся привести к типу runtime::ClassInstance
            if (lhs.TryAs<runtime::ClassInstance>()) {
                runtime::ClassInstance* lhs_instanse = lhs.TryAs<runtime::ClassInstance>();
                // проверяем наличие метода "__eq__"
                if (lhs_instanse->HasMethod(__EQUAL_METHOD__, 1)) {
                    // вызываем метод
                    return lhs_instanse->Call(__EQUAL_METHOD__, {
                        ObjectHolder().Share(*rhs.Get()) }, сontext).TryAs<Bool>()->GetValue();
                }
                else {
                    // в противном случае кидаем исключение
                    throw std::runtime_error("Cannot compare objects for equality"s);
                }
            }
            // пытаемся привести к типу runtime::Bool
            else if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
                return lhs.TryAs<runtime::Bool>()->GetValue() == rhs.TryAs<runtime::Bool>()->GetValue();
            }
            // пытаемся привести к типу runtime::Number
            else if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
                return lhs.TryAs<runtime::Number>()->GetValue() == rhs.TryAs<runtime::Number>()->GetValue();
            }
            // пытаемся привести к типу runtime::String
            else if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
                return lhs.TryAs<runtime::String>()->GetValue() == rhs.TryAs<runtime::String>()->GetValue();
            }
            // если касты не удаются то кидаем исключение
            else {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
        }
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

        // если оба вернули nullptr, сравнивать нечего
        if (!lhs.Get() && !rhs.Get()) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }
        else {
            // пытаемся привести к типу runtime::ClassInstance
            if (lhs.TryAs<runtime::ClassInstance>()) {
                runtime::ClassInstance* lhs_instanse = lhs.TryAs<runtime::ClassInstance>();
                // проверяем наличие метода "__lt__"
                if (lhs_instanse->HasMethod(__LESS_METHOD__, 1)) {
                    // вызываем метод
                    return lhs_instanse->Call(__LESS_METHOD__, {
                        ObjectHolder().Share(*rhs.Get()) }, context).TryAs<Bool>()->GetValue();
                }
                else {
                    // в противном случае кидаем исключение
                    throw std::runtime_error("Cannot compare objects for equality"s);
                }
            }
            // пытаемся привести к типу runtime::Bool
            else if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
                return lhs.TryAs<runtime::Bool>()->GetValue() < rhs.TryAs<runtime::Bool>()->GetValue();
            }
            // пытаемся привести к типу runtime::Number
            else if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
                return lhs.TryAs<runtime::Number>()->GetValue() < rhs.TryAs<runtime::Number>()->GetValue();
            }
            // пытаемся привести к типу runtime::String
            else if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
                return lhs.TryAs<runtime::String>()->GetValue() < rhs.TryAs<runtime::String>()->GetValue();
            }
            // если касты не удаются то кидаем исключение
            else {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
        }
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        // возвращаем что левая часть НЕ равна правой
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        // возвращаем что левая часть НЕ меньше и НЕ равна правой
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        // возвращаем что левая часть НЕ больше чем правая
        return !Greater(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        // возвращаем что левая часть НЕ меньше чем правая
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime
