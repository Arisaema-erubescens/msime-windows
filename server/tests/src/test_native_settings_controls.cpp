#include "tests/includes/test_framework.h"

#include "settings/native/native_settings_controls.h"

#include "msimeui/Window.h"

#include <memory>

namespace
{
using native_settings::Button;

struct HandlerProbe
{
    static int live;
    static int liveDuringCall;

    explicit HandlerProbe(std::shared_ptr<Button> *owner) : owner_(owner)
    {
        ++live;
    }

    HandlerProbe(const HandlerProbe &other) : owner_(other.owner_)
    {
        ++live;
    }

    HandlerProbe &operator=(const HandlerProbe &) = delete;

    ~HandlerProbe()
    {
        --live;
    }

    void operator()() const
    {
        // Closing the confirmation dialog releases its button and the stored callback
        owner_->reset();
        // Only statics are read after teardown, including when the callback has died
        liveDuringCall = live;
    }

  private:
    std::shared_ptr<Button> *owner_;
};

int HandlerProbe::live = 0;
int HandlerProbe::liveDuringCall = -1;

void CheckHandlerLifetime(WPARAM key)
{
    // No HWND is created, so these tests do not open a window or take input focus
    msimeui::Window window(L"settings-controls-tests", L"", 100, 100);
    auto button = std::make_shared<Button>(L"Delete");
    Button *raw = button.get();
    std::weak_ptr<Button> watch = button;
    raw->Attach(&window);
    raw->Arrange({0.0f, 0.0f, 100.0f, 30.0f});
    HandlerProbe::liveDuringCall = -1;
    raw->SetOnClick(HandlerProbe(&button));
    REQUIRE_EQ(HandlerProbe::live, 1);

    if (key)
        REQUIRE(raw->OnKeyDown(key, 0));
    else
    {
        const POINT point{10, 10};
        REQUIRE(raw->OnMouseDown(point, 0));
        REQUIRE(raw->OnMouseUp(point, 0));
    }

    REQUIRE(!button);
    REQUIRE(watch.expired());
    REQUIRE(HandlerProbe::liveDuringCall >= 1);
    REQUIRE_EQ(HandlerProbe::live, 0);
}
} // namespace

TEST_CASE(SettingsButtonMouseHandlerOutlivesTheDialogItCloses)
{
    CheckHandlerLifetime(0);
}

TEST_CASE(SettingsButtonEnterHandlerOutlivesTheDialogItCloses)
{
    CheckHandlerLifetime(VK_RETURN);
}

TEST_CASE(SettingsButtonSpaceHandlerOutlivesTheDialogItCloses)
{
    CheckHandlerLifetime(VK_SPACE);
}

TEST_CASE(SettingsButtonIgnoresDisabledAndOutsideClicks)
{
    msimeui::Window window(L"settings-controls-tests", L"", 100, 100);
    Button button(L"Delete");
    button.Attach(&window);
    button.Arrange({0.0f, 0.0f, 100.0f, 30.0f});
    int calls = 0;
    button.SetOnClick([&calls]() { ++calls; });
    const POINT inside{10, 10};
    const POINT outside{110, 10};

    button.SetDisabled(true);
    REQUIRE(!button.OnMouseDown(inside, 0));
    REQUIRE(!button.OnMouseUp(inside, 0));
    REQUIRE(!button.OnKeyDown(VK_RETURN, 0));
    REQUIRE(!button.OnKeyDown(VK_SPACE, 0));
    button.SetDisabled(false);
    REQUIRE(!button.OnMouseDown(outside, 0));
    REQUIRE(!button.OnMouseUp(inside, 0));
    REQUIRE(button.OnMouseDown(inside, 0));
    REQUIRE(button.OnMouseUp(outside, 0));
    REQUIRE(!button.OnKeyDown(VK_ESCAPE, 0));
    REQUIRE_EQ(calls, 0);

    REQUIRE(button.OnMouseDown(inside, 0));
    REQUIRE(button.OnMouseUp(inside, 0));
    REQUIRE(button.OnKeyDown(VK_RETURN, 0));
    REQUIRE(button.OnKeyDown(VK_SPACE, 0));
    REQUIRE_EQ(calls, 3);
}
