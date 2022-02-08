#define NB_TYPE_CASTER(type, descr)                                            \
    protected:                                                                 \
        type value;                                                            \
                                                                               \
    public:                                                                    \
        static constexpr auto name = descr;                                    \
        static handle cast(type *p, return_value_policy policy, handle parent) {  \
            if (!p)                                                            \
                return none().release();                                       \
            return cast(*p, policy, parent);                                   \
        }                                                                      \
        operator type *() { return &value; }                                   \
        operator type &() { return value; }                                    \
        template <typename T2> using cast_op_type = cast_op_type<T2>;

NAMESPACE_BEGIN(nanobind)

enum class return_value_policy {
    move
};

NAMESPACE_BEGIN(detail)

template <typename T, typename SFINAE = int> struct type_caster {};
template <typename T> using make_caster = type_caster<intrinsic_t<T>>;

template <typename T>
using cast_op_type =
    std::conditional_t<std::is_pointer_v<std::remove_reference_t<T>>,
                       typename std::add_pointer_t<intrinsic_t<T>>,
                       typename std::add_lvalue_reference_t<intrinsic_t<T>>>;

template <typename T>
struct type_caster<T, enable_if_t<std::is_arithmetic_v<T> && !is_std_char_v<T>>> {
    using T0 = std::conditional_t<sizeof(T) <= sizeof(long), long, long long>;
    using T1 = std::conditional_t<std::is_signed_v<T>, T0, std::make_unsigned_t<T0>>;
    using Tp = std::conditional_t<std::is_floating_point_v<T>, double, T1>;
public:
    bool load(handle src, bool convert) {
        Tp value_p;

        if (!src)
            return false;

        if constexpr (std::is_floating_point_v<T>) {
            if (convert || PyFloat_Check(src.ptr()))
                value_p = (Tp) PyFloat_AsDouble(src.ptr());
            else
                return false;
        } else {
            if (PyFloat_Check(src.ptr()) ||
                (!convert && !PyLong_Check(src.ptr()) &&
                 !PyIndex_Check(src.ptr())))
                return false;

            if constexpr (std::is_unsigned_v<Tp>) {
                value_p = sizeof(T) <= sizeof(long)
                              ? (Tp) PyLong_AsUnsignedLong(src.ptr())
                              : (Tp) PyLong_AsUnsignedLongLong(src.ptr());
            } else {
                value_p = sizeof(T) <= sizeof(long)
                              ? (Tp) PyLong_AsLong(src.ptr())
                              : (Tp) PyLong_AsLongLong(src.ptr());
            }
        }

        // Python API reported an error
        bool py_err = value_p == (Tp) -1 && PyErr_Occurred();

        // Check to see if the conversion is valid
        if (py_err || (std::is_integral_v<T> && sizeof(Tp) != sizeof(T) &&
                       value_p != (Tp) (T) value_p)) {
            PyErr_Clear();
            return false;
        }

        value = (T) value_p;
        return true;
    }

    static handle cast(T src, return_value_policy /* policy */,
                       handle /* parent */) {
        if constexpr (std::is_floating_point_v<T>)
            return PyFloat_FromDouble((double) src);
        else if constexpr (std::is_unsigned_v<T> && sizeof(T) <= sizeof(long))
            return PyLong_FromUnsignedLong((unsigned long) src);
        else if constexpr (std::is_signed_v<T> && sizeof(T) <= sizeof(long))
            return PyLong_FromLong((long) src);
        else if constexpr (std::is_unsigned_v<T>)
            return PyLong_FromUnsignedLongLong((unsigned long long) src);
        else if constexpr (std::is_signed_v<T>)
            return PyLong_FromLongLong((long long) src);
        else
            fail("invalid number cast");
    }

    NB_TYPE_CASTER(T, const_name<std::is_integral_v<T>>("int", "float"));
};

template <> class type_caster<std::nullptr_t> {
    bool load(handle src, bool) {
        if (src && src.is_none())
            return true;
        return false;
    }

    static handle cast(std::nullptr_t, return_value_policy /* policy */,
                       handle /* parent */) {
        return none().inc_ref();
    }

    NB_TYPE_CASTER(std::nullptr_t, const_name("None"));
};

NAMESPACE_END(detail)

template <typename T>
object cast(T &&value, return_value_policy policy = return_value_policy::move) {
    detail::make_caster<T> caster;
    return reinterpret_steal<object>(caster.cast(std::forward<T>(value), policy, nullptr));
}

NAMESPACE_END(nanobind)
