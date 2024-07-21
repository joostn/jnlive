#pragma once
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <gtkmm.h>
#include <condition_variable>
#include <regex>

#define nhAssert(_Expression) 							\
     (static_cast <bool> (_Expression)						\
      ? void (0)							\
      : utils::Assert (#_Expression, __FILE__, __LINE__))

namespace utils
{
    [[noreturn]] inline void Assert(const char * Message, const char *File, unsigned Line)
    {
        throw std::runtime_error(std::string("Assertion failed: ") + Message + " at " + File + ":" + std::to_string(Line));
    }

    class finally
    {
    public:
        typedef std::function<void(void)> TFunc;
        inline finally(TFunc &&func)
            : m_Func(std::move(func)) {}
        inline ~finally() noexcept(false)
        {
            if(!m_Dismissed)
            {
                m_Func();
            }
        }
        void Dismiss() noexcept(true)
        {
            m_Dismissed = true;
        }

        private:
        TFunc m_Func;
        bool m_Dismissed = false;
    };

    class NotifySink;
    class NotifySource
    {
        friend NotifySink;
    public:
        NotifySource(const NotifySource&) = delete;
        NotifySource& operator=(const NotifySource&) = delete;
        NotifySource(NotifySource&&) = delete;
        NotifySource& operator=(NotifySource&&) = delete;
        NotifySource() {}
        ~NotifySource();
        void Notify() const;
    private:
        void AddSink(NotifySink *sink)
        {
            m_Sinks.insert(sink);
        }
        void RemoveSink(NotifySink *sink)
        {
            m_Sinks.erase(sink);
        }    
        std::set<NotifySink*> m_Sinks;
    };

    class NotifySink
    {
        friend NotifySource;
    public:
        NotifySink(const NotifySink&) = delete;
        NotifySink& operator=(const NotifySink&) = delete;
        NotifySink(NotifySource &source, std::function<void(void)> &&func) : m_Func(std::move(func)), m_Source(&source)
        {
            m_Source->AddSink(this);
        }
        NotifySink(NotifySink &&src) : m_Func(std::move(src.m_Func)), m_Source(src.m_Source)
        {
            src.m_Source = nullptr;
        }
        NotifySink& operator=(NotifySink&&) = delete;
        ~NotifySink()
        {
            if(m_Source)
            {
                m_Source->RemoveSink(this);
            }
        }

    private:
        void Notify() const
        {
            m_Func();
        }

    private:
        std::function<void(void)> m_Func;
        NotifySource *m_Source = nullptr;
    };

    class THysteresis
    {
    public:
        THysteresis(int hyst, int reduction) : m_Hyst(hyst), m_Reduction(reduction) {}
        int Update(int delta)
        {
            int reduced = 0;
            m_Value += delta;
            if(m_Value > m_Hyst)
            {
                auto v = m_Value - m_Hyst;
                reduced = v / m_Reduction;
                m_Value -= reduced * m_Reduction;
            }
            else if(m_Value < -m_Hyst)
            {
                auto v = m_Value + m_Hyst;
                reduced = v / m_Reduction;
                m_Value -= reduced * m_Reduction;
            }
            return reduced;            
        }
        
    private:
        int m_Hyst;
        int m_Reduction;
        int m_Value = 0;
    };

    std::string generate_random_tempdir();
    // make a regular expression from a string
    // '*' matches any substring
    // '?' matches any character
    std::regex makeSimpleRegex(std::string_view s);

    class TEventLoop;

    class TEventLoopAction
    {
        friend TEventLoop;
    public:
        TEventLoopAction(const TEventLoopAction&) = delete;
        TEventLoopAction& operator=(const TEventLoopAction&) = delete;
        TEventLoopAction(TEventLoopAction&&) = delete;
        TEventLoopAction& operator=(TEventLoopAction&&) = delete;
        ~TEventLoopAction();
        TEventLoopAction(TEventLoop &eventloop, std::function<void(void)> &&func);
        void Signal(); // can be called from other thread
    private:
        std::function<void(void)> m_Func;
        TEventLoop &m_EventLoop;
    };

    // Abstract base class. Use TGtkAppEventLoop or TThreadWithEventLoop
    class TEventLoop
    {
        friend TEventLoopAction;
    public:
        TEventLoop(const TEventLoop&) = delete;
        TEventLoop& operator=(const TEventLoop&) = delete;
        TEventLoop(TEventLoop&&) = delete;
        TEventLoop& operator=(TEventLoop&&) = delete;
        TEventLoop();
        virtual ~TEventLoop() noexcept(false);
        void CheckThreadId() const;
        void ProcessPendingMessages();

    protected:
        void ActionAdded(TEventLoopAction *action);
        void ActionRemoved(TEventLoopAction *action);
        virtual void ActionSignaled(TEventLoopAction *action) = 0;
        void Call(TEventLoopAction *action)
        {
            action->m_Func();
        }

    protected:
        std::thread::id m_OwningThreadId;
        size_t m_NumActions = 0;
        std::mutex m_Mutex;
        std::set<TEventLoopAction*> m_SignaledActions;
    };

    // Event loops which uses the GTK eventloop
    class TGtkAppEventLoop : public TEventLoop
    {
    public:
        TGtkAppEventLoop();
        virtual ~TGtkAppEventLoop();

    protected:
        virtual void ActionSignaled(TEventLoopAction *action) override;

    private:
        Glib::Dispatcher m_Dispatcher;
    };

    // Event loop which uses a separate thread
    class TThreadWithEventLoop : public TEventLoop
    {
    public:
        TThreadWithEventLoop();
        virtual ~TThreadWithEventLoop();
        void Run();
        void Stop();

    protected:
        virtual void ActionSignaled(TEventLoopAction *action) override;

    private:
        std::thread m_Thread;
        std::condition_variable m_QueueCond;
        bool m_RequestTerminate = false;
    };



template <typename T>
class TTypedPoint
{
public:
  typedef TTypedPoint<T> TThis;
  auto operator<=>(const TThis &) const = default;
  inline constexpr TTypedPoint()
    : m_X(0), m_Y(0) {}
  inline constexpr TTypedPoint(T x, T y)
    : m_X(x), m_Y(y) {}
  inline constexpr TTypedPoint(T xy)
    : m_X(xy), m_Y(xy) {}
  inline TTypedPoint(const TTypedPoint<T> &src) = default;
  template <class T2>
  inline TTypedPoint(const TTypedPoint<T2> &src)
    : m_X((T)src.X()), m_Y((T)src.Y()) {}
  inline constexpr T X() const { return m_X; }
  inline constexpr T Y() const { return m_Y; }
  TTypedPoint<T> Floor() const
  {
    if constexpr (std::is_integral_v<T>)
    {
      return *this;
    }
    else
    {
      return TTypedPoint<T>(std::floor(X()), std::floor(Y()));
    }
  }
  TTypedPoint<T> Ceil() const
  {
    if constexpr (std::is_integral_v<T>)
    {
      return *this;
    }
    else
    {
      return TTypedPoint<T>(std::ceil(X()), std::ceil(Y()));
    }
  }
  TTypedPoint<T> Round() const
  {
    if constexpr (std::is_integral_v<T>)
    {
      return *this;
    }
    else
    {
      return TTypedPoint<T>(std::round(X()), std::round(Y()));
    }
  }
  TTypedPoint<int> IRound() const
  {
    if constexpr (std::is_integral_v<T>)
    {
      return TTypedPoint<int>((int)X(), (int)Y());
    }
    else
    {
      return TTypedPoint<int>((int)std::lround(X()), (int)std::lround(Y()));
    }
  }
  TTypedPoint<long long> LLRound() const
  {
    if constexpr (std::is_integral_v<T>)
    {
      return TTypedPoint<long long>((long long)X(), (long long)Y());
    }
    else
    {
      return TTypedPoint<long long>(std::llround(X()), std::llround(Y()));
    }
  }
  TThis Negate() const
  {
    return TThis(-X(), -Y());
  }
  TThis Abs() const
  {
    return{ std::abs(X()), std::abs(Y()) };
  }
  T MaximumNorm() const
  {
    return std::max(std::abs(X()), std::abs(Y()));
  }
  T MinXY() const
  {
    return std::min(X(), Y());
  }
  T MaxXY() const
  {
    return std::max(X(), Y());
  }
  void SetX(const T& x)
  {
    m_X = x;
  }
  void SetY(const T& y)
  {
    m_Y = y;
  }
  void IncrementX(const T& v)
  {
    m_X += v;
  }
  void IncrementY(const T& v)
  {
    m_Y += v;
  }
  void DecrementX(const T& v)
  {
    m_X -= v;
  }
  void DecrementY(const T& v)
  {
    m_Y -= v;
  }
  TTypedPoint<T> operator+(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(X() + src.X(), Y() + src.Y());
    return result;
  }
  TTypedPoint<T> operator-(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(X() - src.X(), Y() - src.Y());
    return result;
  }
  TTypedPoint<T> operator-() const
  {
    TTypedPoint<T> result(-X(), -Y());
    return result;
  }
  TTypedPoint<T> operator*(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(X() * src.X(), Y() * src.Y());
    return result;
  }
  TTypedPoint<T> operator/(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(X() / src.X(), Y() / src.Y());
    return result;
  }
  TTypedPoint<T> operator/(T v) const
  {
    TTypedPoint<T> result(X() / v, Y() / v);
    return result;
  }
  TTypedPoint<T> operator*(T v) const
  {
    TTypedPoint<T> result(X() * v, Y() * v);
    return result;
  }
  TTypedPoint<T> operator+(T v) const
  {
    TTypedPoint<T> result(X() + v, Y() + v);
    return result;
  }
  TTypedPoint<T> operator-(T v) const
  {
    TTypedPoint<T> result(X() - v, Y() - v);
    return result;
  }
  TTypedPoint<T> operator>>(int v) const
  {
    TTypedPoint<T> result(X() >> v, Y() >> v);
    return result;
  }
  TTypedPoint<T> operator<<(int v) const
  {
    TTypedPoint<T> result(X() << v, Y() << v);
    return result;
  }
  TTypedPoint<T>& operator+=(const TTypedPoint<T> &src)
  {
    *this = *this + src;
    return *this;
  }
  TTypedPoint<T>& operator-=(const TTypedPoint<T> &src)
  {
    *this = *this - src;
    return *this;
  }
  TTypedPoint<T>& operator/=(const TTypedPoint<T> &src)
  {
    *this = *this / src;
    return *this;
  }
  TTypedPoint<T>& operator*=(const TTypedPoint<T> &src)
  {
    *this = *this * src;
    return *this;
  }
  TTypedPoint<T>& operator>>=(const TTypedPoint<T> &src)
  {
    *this = *this >> src;
    return *this;
  }
  TTypedPoint<T>& operator<<=(const TTypedPoint<T> &src)
  {
    *this = *this << src;
    return *this;
  }
  TTypedPoint<T>& operator+=(const T &src)
  {
    *this = *this + src;
    return *this;
  }
  TTypedPoint<T>& operator-=(const T &src)
  {
    *this = *this - src;
    return *this;
  }
  TTypedPoint<T> operator>>(const TTypedPoint<T> &src) const
  {
    return TTypedPoint<T>(X() >> src.X(), Y() >> src.Y());
  }
  TTypedPoint<T> operator<<(const TTypedPoint<T> &src) const
  {
    return TTypedPoint<T>(X() << src.X(), Y() << src.Y());
  }
  TTypedPoint<T> Max(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(std::max(X(), src.X()), std::max(Y(), src.Y()));
    return result;
  }
  TTypedPoint<T> Min(const TTypedPoint<T> &src) const
  {
    TTypedPoint<T> result(std::min(X(), src.X()), std::min(Y(), src.Y()));
    return result;
  }
  TTypedPoint<T> SwapXY() const
  {
    return TTypedPoint<T>(Y(), X());
  }
  T HypotSquared() const
  {
    return X() * X() + Y() * Y();
  }

  // all angles:
  // 0 degrees: (1,0)
  // 90 degrees: (0,1)
  // 180 degrees: (-1,0)
  // 270 degrees: (0,-1)
  //RotateBy: if angle > 0 (and y> 0 is top and x > 0 is right) then rotates counterclockwise
  TTypedPoint<T> RotateBySinCos(T sinOfAngle, T cosOfAngle) const
  {
    return{ X() * cosOfAngle - Y() * sinOfAngle, X() * sinOfAngle + Y() * cosOfAngle };
  }
  T InProduct(const TTypedPoint<T> &p) const
  {
    return ((*this)*p).template XYSumAsType<T>();
  }
  T CrossProduct(const TTypedPoint<T> &p) const
  {
    return X() * p.Y() - Y() * p.X();
  }
  TTypedPoint<T> RotateByRadians(T angle) const
  {
    return RotateBySinCos(std::sin(angle), std::cos(angle));
  }
  TTypedPoint<T> RotateByDegrees(T angle) const
  {
    return RotateByRadians(angle * M_PI / 180.0);
  }
  T AngleRadians() const
  {
    return std::atan2(Y(), X());
  }
  T AngleDegrees() const
  {
    return AngleRadians() * T(180.0 / M_PI);
  }
  static TTypedPoint<T> FromAngleRadians(T angle)
  {
    return{ std::cos(angle), std::sin(angle) };
  }
  static TTypedPoint<T> FromAngleDegrees(T angle)
  {
    return FromAngleRadians(angle * T(M_PI / 180.0));
  }
  T Hypot() const
  {
    static_assert(!std::is_integral_v<T>);
    return std::sqrt(HypotSquared());
  }
  template <typename VT>
  VT XYProductAsType() const
  {
    return ((VT)X()) * ((VT)Y());
  }
  template <typename VT>
  VT XYSumAsType() const
  {
    return ((VT)X()) + ((VT)Y());
  }
  T XYProduct() const
  {
    return XYProductAsType<T>();
  }
  template <typename VT>
  VT XDividedByYAsType() const
  {
    return ((VT)X()) / ((VT)Y());
  }
  friend std::ostream& operator<<(std::ostream& os, const TTypedPoint<T>& p)
  {
    os << "(" << p.X() << "," << p.Y() << ")";
    return os;
  }

private:
  T m_X;
  T m_Y;
};

enum class TPointOrSize { Point, Size };

template <typename T>
class TTypedRect
{
public:
  typedef TTypedRect<T> TThis;
  auto operator<=>(const TThis &) const = default;
  constexpr inline TTypedRect() {}
  constexpr inline TTypedRect(const TTypedPoint<T> &topleft, const TTypedPoint<T> &second, TPointOrSize secondparameter)
    : m_TopLeft(topleft)
  {
    if (secondparameter == TPointOrSize::Point)
    {
      m_BottomRight = second;
    }
    else // TPointOrSize::Size
    {
      m_BottomRight = topleft + second;
    }
  }

  constexpr inline TTypedRect(const TTypedRect<T> &src) = default;
  constexpr static TThis FromCenterAndSize(const TTypedPoint<T> &center, const TTypedPoint<T> &size)
  {
    auto halfsize = size / ((T)2);
    return FromTopLeftAndSize(center - halfsize, size);
  }
  constexpr static TThis FromTopLeftAndSize(const TTypedPoint<T> &topleft, const TTypedPoint<T> &size)
  {
    return TTypedRect(topleft, size, TPointOrSize::Size);
  }
  constexpr static TThis FromTopLeftAndBottomRight(const TTypedPoint<T> &topleft, const TTypedPoint<T> &bottomright)
  {
    return TTypedRect(topleft, bottomright, TPointOrSize::Point);
  }
  constexpr static TThis FromSize(const TTypedPoint<T> &size)
  {
    return TTypedRect(TTypedPoint<T>(), size, TPointOrSize::Point);
  }
  template <class T2>
  TTypedRect(const TTypedRect<T2> &src)
    : m_TopLeft((TTypedPoint<T>)src.TopLeft()), m_BottomRight((TTypedPoint<T>)src.BottomRight()) {}
  TTypedRect<T> Round() const
  {
    return TTypedRect<T>(TopLeft().Round(), BottomRight().Round(), TPointOrSize::Point);
  }
  TTypedRect<int> IRound() const
  {
    return TTypedRect<int>(TopLeft().IRound(), BottomRight().IRound(), TPointOrSize::Point);
  }
  TTypedRect<long long> LLRound() const
  {
    return TTypedRect<long long>(TopLeft().LLRound(), BottomRight().LLRound(), TPointOrSize::Point);
  }
  // like ToRect, but rounds to the outer integer rectangle (i.e. right=0.01 becomes 1)
  TTypedRect<T> FloorCeil() const
  {
    return TTypedRect<T>(TopLeft().Floor(), BottomRight().Ceil(), TPointOrSize::Point);
  }
  // like ToRect, but rounds to the inner integer rectangle (i.e. right=0.01 becomes 1)
  TTypedRect<T> CeilFloor() const
  {
    return TTypedRect<T>(TopLeft().Ceil(), BottomRight().Floor(), TPointOrSize::Point);
  }
  TTypedRect<int> ToRectOuterPixels() const
  {
    return TTypedRect<int>(FloorCeil());
  }
  TTypedRect<int> ToRectInnerPixels() const
  {
    return TTypedRect<int>(CeilFloor());
  }
  const TTypedPoint<T>& TopLeft() const { return m_TopLeft; }
  const TTypedPoint<T>& BottomRight() const { return m_BottomRight; }
  TTypedPoint<T> TopRight() const { return TTypedPoint<T>(BottomRight().X(), TopLeft().Y()); }
  TTypedPoint<T> BottomLeft() const { return TTypedPoint<T>(TopLeft().X(), BottomRight().Y()); }
  TTypedPoint<T> LeftCenter() const { return{ Left(), Center().Y() }; }
  TTypedPoint<T> RightCenter() const { return{ Right(), Center().Y() }; }
  TTypedPoint<T> CenterTop() const { return{ Center().X(), Top() }; }
  TTypedPoint<T> CenterBottom() const { return{ Center().X(), Bottom() }; }
  TTypedPoint<T> Size() const { return m_BottomRight - m_TopLeft; }
  T Width() const { return Size().X(); }
  T Height() const { return Size().Y(); }
  void SetTopLeft(const TTypedPoint<T>& TopLeft) { m_TopLeft = TopLeft; }
  void SetBottomRight(const TTypedPoint<T>& BottomRight) { m_BottomRight = BottomRight; }
  void SetSize(const TTypedPoint<T>& Size) { m_BottomRight = m_TopLeft + Size; }
  void SetWidth(const T& val) { m_BottomRight.SetX(m_TopLeft.X() + val); }
  void SetHeight(const T& val) { m_BottomRight.SetY(m_TopLeft.Y() + val); }
  T Top() const { return m_TopLeft.Y(); }
  T Left() const { return m_TopLeft.X(); }
  T Right() const { return m_BottomRight.X(); }
  T Bottom() const { return m_BottomRight.Y(); }
  void SetTopKeepBottom(const T& v) { m_TopLeft.SetY(v); }
  void SetLeftKeepRight(const T& v) { m_TopLeft.SetX(v); }
  void SetTopKeepHeight(const T& v) { auto h = Height(); SetTopKeepBottom(v); SetHeight(h); }
  void SetLeftKeepWidth(const T& v) { auto w = Width(); SetLeftKeepRight(v); SetWidth(w); }
  void SetBottom(const T& v) { m_BottomRight.SetY(v); }
  void SetRight(const T& v) { m_BottomRight.SetX(v); }
  void IncrementTopLeft(const TTypedPoint<T> &delta)
  {
    m_TopLeft += delta;
  }
  void IncrementBottomRight(const TTypedPoint<T> &delta)
  {
    m_BottomRight += delta;
  }
  void ShiftRight(const T&v)
  {
    m_TopLeft.IncrementX(v);
    m_BottomRight.IncrementX(v);
  }
  void ShiftLeft(const T&v)
  {
    ShiftRight(-v);
  }
  void ShiftDown(const T&v)
  {
    m_TopLeft.IncrementY(v);
    m_BottomRight.IncrementY(v);
  }
  void ShiftUp(const T&v)
  {
    ShiftDown(-v);
  }
  void IncreaseWidth(const T&v)
  {
    m_BottomRight.IncrementX(v);
  }
  void DecreaseWidth(const T&v)
  {
    IncreaseWidth(-v);
  }
  void IncreaseHeight(const T&v)
  {
    m_BottomRight.IncrementY(v);
  }
  void DecreaseHeight(const T&v)
  {
    IncreaseHeight(-v);
  }
  TTypedRect<T> operator-(const TTypedPoint<T>& v) const
  {
    return TTypedRect(TopLeft() - v, BottomRight() - v, TPointOrSize::Point);
  }
  TTypedRect<T> operator+(const TTypedPoint<T>& v) const
  {
    return TTypedRect(TopLeft() + v, BottomRight() + v, TPointOrSize::Point);
  }
  TTypedRect<T> operator-(const T &v) const
  {
    return TTypedRect(TopLeft() - v, BottomRight() - v, TPointOrSize::Point);
  }
  TTypedRect<T> operator+(const T &v) const
  {
    return TTypedRect(TopLeft() + v, BottomRight() + v, TPointOrSize::Point);
  }
  TTypedRect<T> operator*(const TTypedPoint<T> &v) const
  {
    return TTypedRect(TopLeft() * v, BottomRight() * v, TPointOrSize::Point);
  }
  TTypedRect<T> operator*(const T &v) const
  {
    return TTypedRect(TopLeft() * v, BottomRight() * v, TPointOrSize::Point);
  }
  TTypedRect<T> operator/(const TTypedPoint<T> &v) const
  {
    return TTypedRect(TopLeft() / v, BottomRight() / v, TPointOrSize::Point);
  }
  TTypedRect<T> operator/(const T &v) const
  {
    return TTypedRect(TopLeft() / v, BottomRight() / v, TPointOrSize::Point);
  }
  TTypedRect<T>& operator-=(const TTypedPoint<T>& v)
  {
    *this = *this - v;
    return *this;
  }
  TTypedRect<T>& operator+=(const TTypedPoint<T>& v)
  {
    *this = *this + v;
    return *this;
  }
  TTypedRect<T>& operator-=(const T& v)
  {
    *this = *this - v;
    return *this;
  }
  TTypedRect<T>& operator+=(const T& v)
  {
    *this = *this + v;
    return *this;
  }
  TTypedRect<T>& operator*=(const TTypedPoint<T>& v)
  {
    *this = *this * v;
    return *this;
  }
  TTypedRect<T>& operator*=(const T& v)
  {
    *this = (*this) * v;
    return *this;
  }
  TTypedRect<T>& operator/=(const TTypedPoint<T>& v)
  {
    *this = *this / v;
    return *this;
  }
  TTypedRect<T>& operator/=(const T& v)
  {
    *this = (*this) / v;
    return *this;
  }
  TTypedPoint<T> Center() const
  {
    return (TopLeft() + BottomRight()) / ((T)2);
  }
  bool IsEmpty() const
  {
    return (Left() >= Right()) || (Top() >= Bottom());
  }
  void MakeEmpty()
  {
    SetTopLeft({ 0 });
    SetBottomRight({ 0 });
  }
  // result is this rectangle shifted such that it is contained in container
  TTypedRect<T> MoveInside(const TTypedRect<T> &container) const
  {
    TTypedRect<T> result = *this;
    if (result.Right() > container.Right())
    {
      result.ShiftLeft(result.Right() - container.Right());
    }
    if (result.Left() < container.Left())
    {
      result.ShiftRight(container.Left() - result.Left());
    }
    if (result.Bottom() > container.Bottom())
    {
      result.ShiftUp(result.Bottom() - container.Bottom());
    }
    if (result.Top() < container.Top())
    {
      result.ShiftDown(container.Top() - result.Top());
    }
    return result;
  }
  TTypedPoint<T> MovePointInsideRect(const TTypedPoint<T> &point) const
  {
    return point.Max(TopLeft()).Min(BottomRight());
  }
  TTypedRect<T> SymmetricalExpand(const TTypedPoint<T> &deltasize) const
  {
    return TTypedRect<T>::FromTopLeftAndBottomRight(TopLeft() - deltasize, BottomRight() + deltasize);
  }
  bool Intersects(const TTypedRect<T> &src) const
  {
    if (IsEmpty()) return false;
    if (src.IsEmpty()) return false;
    if (src.Left() >= Right()) return false;
    if (src.Right() <= Left()) return false;
    if (src.Top() >= Bottom()) return false;
    if (src.Bottom() <= Top()) return false;
    return true;
  }
  TTypedRect<T> Intersection(const TTypedRect<T> &src) const
  {
    if (IsEmpty() || src.IsEmpty()) return TTypedRect<T>();
    TTypedRect<T> result(TopLeft().Max(src.TopLeft()), BottomRight().Min(src.BottomRight()), TPointOrSize::Point);
    if (result.IsEmpty())
    {
      result.MakeEmpty();
    }
    return result;
  }
  TTypedRect<T> Union(const TTypedRect<T> &src) const
  {
    if (src.IsEmpty())
    {
      if (IsEmpty())
      {
        return TTypedRect<T>();
      }
      else
      {
        return *this;
      }
    }
    if (IsEmpty()) return src;
    TTypedRect<T> result(TopLeft().Min(src.TopLeft()), BottomRight().Max(src.BottomRight()), TPointOrSize::Point);
    return result;
  }
  int Subtract(const TTypedRect<T> &sub, TTypedRect<T> result[4]) const
  {
    TTypedRect<T> sub_intersected = Intersection(sub);
    int resultsize = 0;
    if (!IsEmpty())
    {
      if (sub_intersected.IsEmpty())
      {
        result[resultsize++] = *this;
      }
      else
      {
        if (Top() < sub_intersected.Top())
        {
          result[resultsize++] = FromTopLeftAndBottomRight({Left(), Top()}, {Right(), sub_intersected.Top()});
        }
        if (Left() < sub_intersected.Left())
        {
          result[resultsize++] = FromTopLeftAndBottomRight({Left(), sub_intersected.Top()}, {sub_intersected.Left(), sub_intersected.Bottom()});
        }
        if (Right() > sub_intersected.Right())
        {
          result[resultsize++] = FromTopLeftAndBottomRight({sub_intersected.Right(), sub_intersected.Top()}, {Right(), sub_intersected.Bottom()});
        }
        if (Bottom() > sub_intersected.Bottom())
        {
          result[resultsize++] = FromTopLeftAndBottomRight({Left(), sub_intersected.Bottom()}, {Right(), Bottom()});
        }
      }
    }
    return resultsize;
  }
  void Fit(TTypedPoint<T> &origin, TTypedPoint<T> &size, TTypedPoint<T> &deltaorigin) const
  {
    deltaorigin = (TopLeft() - origin).Max(TTypedPoint<T>(0, 0));
    origin += deltaorigin;
    size -= deltaorigin;
    TTypedPoint<T> newbottomright = (origin + size).Min(BottomRight());
    size = newbottomright - origin;
  }
  static void Fit2(TTypedPoint<T> &origin1, const TTypedRect<T> &rect1, TTypedPoint<T> &origin2, const TTypedRect<T> &rect2, TTypedPoint<T> &size)
  {
    TTypedPoint<T> deltaorigin;
    rect1.Fit(origin1, size, deltaorigin);
    origin2 += deltaorigin;
    rect2.Fit(origin2, size, deltaorigin);
    origin1 += deltaorigin;
  }
  bool Contains(const TTypedRect<T> &needle) const
  {
    // ambiguous: what if both are empty?
    if (needle.IsEmpty()) return true;
    if (IsEmpty()) return false;
    if (needle.Left() < Left()) return false;
    if (needle.Right() > Right()) return false;
    if (needle.Top() < Top()) return false;
    if (needle.Bottom() > Bottom()) return false;
    return true;
  }
  bool Contains(const TTypedPoint<T> &needle) const
  {
    return (needle.X() >= TopLeft().X())
      && (needle.Y() >= TopLeft().Y())
      && (needle.X() < BottomRight().X())
      && (needle.Y() < BottomRight().Y());
  }
  friend std::ostream& operator<<(std::ostream& os, const TTypedRect<T>& p)
  {
    os << "(" << p.TopLeft() << "-" << p.BottomRight() << ")";
    return os;
  }


private:
  TTypedPoint<T> m_TopLeft;
  TTypedPoint<T> m_BottomRight;
};


typedef TTypedPoint<int> TIntPoint;
typedef TTypedPoint<int> TIntSize;
typedef TTypedPoint<float> TFloatPoint;
typedef TTypedPoint<double> TDoublePoint;
typedef TTypedRect<float> TFloatRect;
typedef TTypedRect<int> TIntRect;
typedef TTypedRect<double> TDoubleRect;

/*

TTypedRegion<int> emptyregion;
TTypedRegion<int> region1(TIntRect::FromTopLeftAndBottomRight({10,20},{200,300}));
We can do Union(), Intersection() and Difference(). These return a new TTypedRegion.
Get the rectangles using forEach():

region1.forEach([&](const TIntRect &r}{
 // do something
});

*/

template <typename T>
class TTypedRegion
{
public:
  class THorizontalRange
  {
  public:
    THorizontalRange(T left, T right)
      : m_Left(left), m_Right(right)
    {
      nhAssert(left < right);
    }
    inline T Left() const { return m_Left; }
    inline T Right() const { return m_Right; }
    THorizontalRange Shift(T deltaX) const
    {
      return THorizontalRange(Left() + deltaX, Right() + deltaX);
    }
    auto operator<=>(const THorizontalRange&) const = default;

  private:
    T m_Left;
    T m_Right;
  };

  class TVerticalRange
  {
  public:
    TVerticalRange(T top, T bottom)
      : m_Top(top), m_Bottom(bottom)
    {
      nhAssert(top < bottom);
    }
    TVerticalRange(T top, T bottom, const std::vector<THorizontalRange> &horzRanges)
      : m_Top(top), m_Bottom(bottom), m_HorzRanges(horzRanges)
    {
      nhAssert(top < bottom);
    }
    void AppendHorzRange(const THorizontalRange &horzrange)
    {
      if (!m_HorzRanges.empty())
      {
        nhAssert(m_HorzRanges.back().Right() <= horzrange.Left());
        if (m_HorzRanges.back().Right() == horzrange.Left())
        {
          m_HorzRanges.back() = THorizontalRange(m_HorzRanges.back().Left(), horzrange.Right());
        }
        else
        {
          m_HorzRanges.push_back(horzrange);
        }
      }
      else
      {
        m_HorzRanges.push_back(horzrange);
      }
    }
    inline T Top() const { return m_Top; }
    inline T Bottom() const { return m_Bottom; }
    inline void SetTop(const T &val) { m_Top = val; }
    inline void SetBottom(const T &val) { m_Bottom = val; }
    inline const std::vector<THorizontalRange>& HorzRanges() const { return m_HorzRanges; }
    TVerticalRange Shift(const TTypedPoint<T> &delta) const
    {
      TVerticalRange result(Top() + delta.Y(), Bottom() + delta.Y());
      for (const auto &hrange : m_HorzRanges)
      {
        m_HorzRanges.push_back(hrange.Shift(delta.X()));
      }
      return result;
    }

  private:
    T m_Top;
    T m_Bottom;
    std::vector<THorizontalRange> m_HorzRanges;
  };

public:
  inline TTypedRegion() {}
  inline TTypedRegion(const TTypedRegion& src) : m_VertRanges(src.m_VertRanges) {}
  TTypedRegion(const TTypedRect<T> &r)
  {
    if (!r.IsEmpty())
    {
      TVerticalRange newrange(r.Top(), r.Bottom());
      newrange.AppendHorzRange({ r.Left(), r.Right() });
      AppendRangeAtBottom(std::move(newrange));
    }
  }
  inline TTypedRegion(TTypedRegion&& src) : m_VertRanges(std::move(src.m_VertRanges)) {}
  inline TTypedRegion<T>& operator=(const TTypedRegion& src)
  {
    m_VertRanges = src.m_VertRanges;
    return *this;
  }
  inline TTypedRegion<T>& operator=(TTypedRegion&& src)
  {
    if (this != &src)
    {
      m_VertRanges = std::move(src.m_VertRanges);
    }
    return *this;
  }
  inline bool empty() const
  {
    return m_VertRanges.empty();
  }

  TTypedRegion Union(const TTypedRegion<T> &other) const
  {
    TTypedRegion result;
    IterateVert(m_VertRanges, other.m_VertRanges, [&](T top, T bottom, const std::vector<THorizontalRange> &horzrange1, const std::vector<THorizontalRange> &horzrange2) {
      TVerticalRange newvertrange(top, bottom);
      IterateHorz(horzrange1, horzrange2, [&](T left, T right, bool /*has1*/, bool /*has2*/) {
        newvertrange.AppendHorzRange({ left, right });
      });
      result.AppendRangeAtBottom(std::move(newvertrange));
    });
    return result;
  }
  TTypedRegion Subtract(const TTypedRegion<T> &other) const
  {
    TTypedRegion result;
    IterateVert(m_VertRanges, other.m_VertRanges, [&](T top, T bottom, const std::vector<THorizontalRange> &horzrange1, const std::vector<THorizontalRange> &horzrange2) {
      TVerticalRange newvertrange(top, bottom);
      IterateHorz(horzrange1, horzrange2, [&](T left, T right, bool has1, bool has2) {
        if (has1 && (!has2))
        {
          newvertrange.AppendHorzRange({ left, right });
        }
      });
      result.AppendRangeAtBottom(std::move(newvertrange));
    });
    return result;
  }
  TTypedRegion Intersection(const TTypedRegion<T> &other) const
  {
    TTypedRegion result;
    IterateVert(m_VertRanges, other.m_VertRanges, [&](T top, T bottom, const std::vector<THorizontalRange> &horzrange1, const std::vector<THorizontalRange> &horzrange2) {
      TVerticalRange newvertrange(top, bottom);
      IterateHorz(horzrange1, horzrange2, [&](T left, T right, bool has1, bool has2) {
        if (has1 && has2)
        {
          newvertrange.AppendHorzRange({ left, right });
        }
      });
      result.AppendRangeAtBottom(std::move(newvertrange));
    });
    return result;
  }
  bool Intersects(const TTypedRegion<T> &other) const
  {
    bool result = false;
    IterateVert(m_VertRanges, other.m_VertRanges, [&](T /*top*/, T /*bottom*/, const std::vector<THorizontalRange> &horzrange1, const std::vector<THorizontalRange> &horzrange2) {
      IterateHorz(horzrange1, horzrange2, [&](T /*left*/, T /*right*/, bool has1, bool has2) {
        if (has1 && has2) result = true;
      });
    });
    return result;
  }
  bool Contains(const TTypedRegion<T> &other) const
  {
    bool result = true;
    IterateVert(m_VertRanges, other.m_VertRanges, [&](T /*top*/, T /*bottom*/, const std::vector<THorizontalRange> &horzrange1, const std::vector<THorizontalRange> &horzrange2) {
      IterateHorz(horzrange1, horzrange2, [&](T /*left*/, T /*right*/, bool has1, bool has2) {
        if ( (!has1) && has2) result = false;
      });
    });
    return result;
  }
  bool Contains(const TTypedPoint<T> &other) const
  {
    bool result = false;
    ForEach([&](const TTypedRect<T> &rect) {
      if (rect.Contains(other)) result = true;
    });
    return result;
  }
  template <class Function>
  void ForEach(const Function &func) const
  {
    for (const auto &vertrange : m_VertRanges)
    {
      for (const auto &horzrange : vertrange.HorzRanges())
      {
        auto r = TTypedRect<T>::FromTopLeftAndBottomRight({ horzrange.Left(), vertrange.Top() }, { horzrange.Right(), vertrange.Bottom() });
        func(r);
      }
    }
  }
  TTypedRegion<T> Shift(const TTypedPoint<T> &delta) const
  {
    TTypedRegion<T> result;
    for (const auto &vrange : m_VertRanges)
    {
      result.m_VertRanges.push_back(vrange.Shift(delta));
    }
    return result;
  }
  TTypedRect<T> OuterBounds() const
  {
    TTypedRect<T> result;
    ForEach([&](const TTypedRect<T> &rect) {
      result = result.Union(rect);
    });
    return result;
  }
  const std::vector<TVerticalRange> &VertRanges() const {return m_VertRanges;}

private:
  template <class Function>
  static void IterateVert(const std::vector<TVerticalRange> &range1, const std::vector<TVerticalRange> &range2, const Function &func)
  {
    T y = 0;
    auto it1 = range1.begin();
    auto it2 = range2.begin();
    bool atend1 = (it1 == range1.end());
    bool atend2 = (it2 == range2.end());
    if (atend1)
    {
      if (atend2)
      {
        return;
      }
      else
      {
        y = it2->Top();
      }
    }
    else
    {
      y = it1->Top();
      if (!atend2)
      {
        if (it2->Top() < y)
        {
          y = it2->Top();
        }
      }
    }
    while (true)
    {
      atend1 = (it1 == range1.end());
      atend2 = (it2 == range2.end());
      if (atend1 && atend2) break;
      bool contains1 = (!atend1) && (y >= it1->Top());
      bool contains2 = (!atend2) && (y >= it2->Top());
      int nexty = 0;
      if (!atend1)
      {
        if (contains1)
        {
          nexty = it1->Bottom();
        }
        else
        {
          nexty = it1->Top();
        }
      }
      if (!atend2)
      {
        if (contains2)
        {
          if (!atend1)
          {
            nexty = std::min(nexty, it2->Bottom());
          }
          else
          {
            nexty = it2->Bottom();
          }
        }
        else
        {
          if (!atend1)
          {
            nexty = std::min(nexty, it2->Top());
          }
          else
          {
            nexty = it2->Top();
          }
        }
      }
      nhAssert(nexty > y);
      if (contains1)
      {
        if (contains2)
        {
          func(y, nexty, it1->HorzRanges(), it2->HorzRanges());
        }
        else
        {
          func(y, nexty, it1->HorzRanges(), std::vector<THorizontalRange>());
        }
      }
      else if (contains2)
      {
        func(y, nexty, std::vector<THorizontalRange>(), it2->HorzRanges());
      }
      else
      {
        // nothing here
      }
      y = nexty;
      if ((!atend1) && (y >= it1->Bottom())) it1++;
      if ((!atend2) && (y >= it2->Bottom())) it2++;
    } // while true  
  }

  template <class Function>
  static void IterateHorz(const std::vector<THorizontalRange> &range1, const std::vector<THorizontalRange> &range2, const Function &func)
  {
    T x;
    auto it1 = range1.begin();
    auto it2 = range2.begin();
    bool atend1 = (it1 == range1.end());
    bool atend2 = (it2 == range2.end());
    if (atend1)
    {
      if (atend2)
      {
        return;
      }
      else
      {
        x = it2->Left();
      }
    }
    else
    {
      if (atend2)
      {
        x = it1->Left();
      }
      else
      {
        x = std::min(it1->Left(), it2->Left());
      }
    }
    while (true)
    {
      atend1 = (it1 == range1.end());
      atend2 = (it2 == range2.end());
      if (atend1 && atend2) break;
      bool contains1 = (!atend1) && (x >= it1->Left());
      bool contains2 = (!atend2) && (x >= it2->Left());
      int nextx = 0;
      if (!atend1)
      {
        if (contains1)
        {
          nextx = it1->Right();
        }
        else
        {
          nextx = it1->Left();
        }
      }
      if (!atend2)
      {
        if (contains2)
        {
          if (!atend1)
          {
            nextx = std::min(nextx, it2->Right());
          }
          else
          {
            nextx = it2->Right();
          }
        }
        else
        {
          if (!atend1)
          {
            nextx = std::min(nextx, it2->Left());
          }
          else
          {
            nextx = it2->Left();
          }
        }
      }
      nhAssert(nextx > x);
      if (contains1)
      {
        if (contains2)
        {
          func(x, nextx, true, true);
        }
        else
        {
          func(x, nextx, true, false);
        }
      }
      else if (contains2)
      {
        func(x, nextx, false, true);
      }
      else
      {
        // nothing here
      }
      x = nextx;
      if ((!atend1) && (x >= it1->Right())) it1++;
      if ((!atend2) && (x >= it2->Right())) it2++;
    } // while true
  }
  void AppendRangeAtBottom(TVerticalRange &&newvertrange)
  {
    if (!newvertrange.HorzRanges().empty())
    {
      bool done = false;
      if (!m_VertRanges.empty())
      {
        auto &lastrange = m_VertRanges.back();
        nhAssert(lastrange.Bottom() <= newvertrange.Top());
        if (lastrange.Bottom() == newvertrange.Top())
        {
          // directly adjacent:
          if (lastrange.HorzRanges() == newvertrange.HorzRanges())
          {
            // and same horzrange. Just extend:
            lastrange.SetBottom(newvertrange.Bottom());
            done = true;
          }
        }
      }
      if (!done)
      {
        m_VertRanges.push_back(std::move(newvertrange));
      }
    }
  }

private:
  std::vector<TVerticalRange> m_VertRanges;
};

typedef TTypedRegion<float> TFloatRegion;
typedef TTypedRegion<int> TIntRegion;
typedef TTypedRegion<double> TDoubleRegion;

template<class T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
inline T jnMaxValueForIntOrOneForFloat()
{
  return (T)1.0;
}

template<class T, typename std::enable_if<!std::is_floating_point<T>::value, int>::type = 0>
inline T jnMaxValueForIntOrOneForFloat()
{
  return std::numeric_limits<T>::max();
}

template <typename T>
class TTypedColor
{
public:
  typedef TTypedColor<T> TThis;
  inline TTypedColor(const T &red, const T &green, const T &blue, const T &alpha = jnMaxValueForIntOrOneForFloat<T>())
    : m_Channels{ { red, green, blue, alpha } }
  {
  }
  inline TTypedColor()
    : TTypedColor(0, 0, 0, 0) {}
  static inline TThis Grey(const T &h, const T &alpha = jnMaxValueForIntOrOneForFloat<T>())
  {
    return TThis(h, h, h, alpha);
  }
  static inline TThis Black(const T &alpha = jnMaxValueForIntOrOneForFloat<T>())
  {
    return Grey(0, alpha);
  }
  static inline TThis White(const T &alpha = jnMaxValueForIntOrOneForFloat<T>())
  {
    return Grey(MaxValue(), alpha);
  }
  static inline TThis Transparent()
  {
    return Grey(0, 0);
  }
  inline static T MaxValue()
  {
    return jnMaxValueForIntOrOneForFloat<T>();
  }
  auto operator <=> (const TThis&) const = default;
  inline T Red() const { return m_Channels[0]; }
  inline T Green() const { return m_Channels[1]; }
  inline T Blue() const { return m_Channels[2]; }
  inline T Alpha() const { return m_Channels[3]; }
  inline void SetRed(const T &v) { m_Channels[0] = v; }
  inline void SetGreen(const T &v) { m_Channels[1] = v; }
  inline void SetBlue(const T &v) { m_Channels[2] = v; }
  inline void SetAlpha(const T &v) { m_Channels[3] = v; }
  TTypedColor<T> PremultiplyAlpha() const
  {
    static_assert(std::is_floating_point<T>::value, "");
    return TTypedColor<T>(Red()*Alpha(), Green()*Alpha(), Blue()*Alpha(), Alpha());
  }
  TTypedColor<T> PostDivideAlpha() const
  {
    static_assert(std::is_floating_point<T>::value, "");
    T invalpha = 1;
    if(Alpha() != 0)
    {
      invalpha = 1/Alpha();
    }
    return TTypedColor<T>(Red()*invalpha, Green()*invalpha, Blue()*invalpha, Alpha());
  }
  TTypedColor<T> OverlayedBy(const TTypedColor<T> &color) const
  {
    return color.Over(*this);
  }
  TTypedColor<T> Over(const TTypedColor<T> &bottomcolor) const
  {
    auto bottom_premul = bottomcolor.PremultiplyAlpha();
    auto top_premul = PremultiplyAlpha();
    auto invTopAlpha = 1-top_premul.Alpha();
    auto result_premul = TTypedColor<T>(bottom_premul.Red() * invTopAlpha + top_premul.Red(),
                                          bottom_premul.Green() * invTopAlpha + top_premul.Green(),
                                          bottom_premul.Blue() * invTopAlpha + top_premul.Blue(),
                                          bottom_premul.Alpha() * invTopAlpha + top_premul.Alpha());
    return result_premul.PostDivideAlpha();
  }
//  TColorGradient OverlayedBy(const TColorGradient &gradient) const;
//  TColorGradient Over(const TColorGradient &gradient) const;
  TTypedColor<T> Interpolate(const TTypedColor<T> &othercolor, T d) const
  {
    static_assert(std::is_floating_point<T>::value, "");
    d = std::max((T)0, std::min((T)1, d));
    T invd = 1 - d;
    auto result = TTypedColor<T>(Red()*invd + othercolor.Red()*d,
      Green()*invd + othercolor.Green()*d,
      Blue()*invd + othercolor.Blue()*d,
      Alpha()*invd + othercolor.Alpha());
    return result;
  }
  static constexpr float HueBlue = 240.0f / 360.0f;
  static constexpr float HueRed = 0.0f / 360.0f;
  static constexpr float HueGreen = 120.0f / 360.0f;
  static constexpr float HueCyan = 180.0f / 360.0f;
  static constexpr float HueYellow = 60.0f / 360.0f;
  static constexpr float HuePurple = 300.0f / 360.0f;
  static TTypedColor<T> HSV(T h, T s, T v, T alpha = 1)
  {
    static_assert(std::is_floating_point<T>::value, "");
    TTypedColor<T> result;
    result.SetAlpha(alpha);
    if (s == 0)
    {
      result.SetRed(v);
      result.SetGreen(v);
      result.SetBlue(v);
    }
    else
    {
      T var_h = h * 6;
      T var_i = std::floor(var_h);
      T var_1 = v * (1 - s);
      T var_2 = v * (1 - s * (var_h - var_i));
      T var_3 = v * (1 - s * (1 - (var_h - var_i)));

      if (var_i == 0) { result.SetRed(v); result.SetGreen(var_3); result.SetBlue(var_1); }
      else if (var_i == 1) { result.SetRed(var_2); result.SetGreen(v); result.SetBlue(var_1); }
      else if (var_i == 2) { result.SetRed(var_1); result.SetGreen(v); result.SetBlue(var_3); }
      else if (var_i == 3) { result.SetRed(var_1); result.SetGreen(var_2); result.SetBlue(v); }
      else if (var_i == 4) { result.SetRed(var_3); result.SetGreen(var_1); result.SetBlue(v); }
      else { result.SetRed(v); result.SetGreen(var_1); result.SetBlue(var_2); }
    }
    return result;
  }
  TTypedColor<T> MultiplyAlphaBy(T f) const
  {
    static_assert(std::is_floating_point<T>::value, "");
    return TTypedColor<T>(Red(), Green(), Blue(), Alpha() * f);
  }
  TTypedColor<T> Desaturate(T f = 1) const
  {
    static_assert(std::is_floating_point<T>::value, "");
    auto sum = Red() + Green() + Blue();
    sum /= 3;
    sum *= f;
    auto invf = T(1) - f;
    return TTypedColor<T>(sum + Red()*invf, sum + Green()*invf, sum + Blue()*invf, Alpha());
  }
  const std::array<T, 4> &Channels() const { return m_Channels; }
  std::array<T, 4> &Channels() { return m_Channels; }
  TTypedColor<T> operator+(const TTypedColor &other) const
  {
    return TTypedColor<T>(Red() + other.Red(), Green() + other.Green(), Blue() + other.Blue(), Alpha() + other.Alpha());
  }
  TTypedColor<T>& operator+=(const TTypedColor &other)
  {
    *this = *this + other;
    return *this;
  }

private:
  std::array<T,4> m_Channels;
};

using TFloatColor = TTypedColor<float>;

class TUInt8Color : public TTypedColor<unsigned char>
{
public:
  typedef unsigned char T;
  typedef TUInt8Color TThis;
  typedef TTypedColor<T> TParent;
  inline TUInt8Color() {}
  inline TUInt8Color(const TTypedColor<float> &fcolor)
  :TUInt8Color(DoubleToUint8(fcolor.Red()),
                 DoubleToUint8(fcolor.Green()),
                 DoubleToUint8(fcolor.Blue()),
                 DoubleToUint8(fcolor.Alpha())) {}
  inline TUInt8Color(const TTypedColor<unsigned char> &ccolor)
    : TTypedColor<unsigned char>(ccolor) {}
  inline TTypedColor<float> ToFloat() const
  {
    return TTypedColor<float>(Red() / 255.0f, Green() / 255.0f, Blue() / 255.0f, Alpha() / 255.0f);
  }
  inline TUInt8Color(const T &red, const T &green, const T &blue, const T &alpha = jnMaxValueForIntOrOneForFloat<T>())
    : TParent(red, green, blue, alpha) {}
  inline TUInt8Color(int red, int green, int blue, int alpha = jnMaxValueForIntOrOneForFloat<T>())
    : TParent((T)red, (T)green, (T)blue, (T)alpha) {}
  inline TUInt8Color(unsigned int v)
    : TUInt8Color((T)(v & 0xff), (T)((v >> 8) & 0xff), (T)((v >> 16) & 0xff), (T)((v >> 24) & 0xff)) {}
  static unsigned char DoubleToUint8(double v)
  {
    v = std::max(v, 0.0);
    v = std::min(v, 1.0);
    long l = std::lround(v*255.0);
    return (unsigned char)l;
  }
};

    // convert between wstring and string
    // wstring is assumed to be utf16 (even if sizeof(wchar_t) == 4)
    // string is utf8
    std::string wu8(std::wstring_view utf16_str);
    std::wstring u8w(std::string_view utf8_str);
}