# UI ownership

| Surface | Renderer | Owner |
|---|---|---|
| Candidate, floating toolbar, tray menu | msimeui native Direct2D/DirectWrite | `src/window/` presenters in the Server process |
| Settings | msimeui native Direct2D/DirectWrite | `src/settings/native/` in the separate `MetasequoiaImeSettings.exe` |
| Keyboard, handwriting, emoji and voice panels | Their existing native application/presenter | Shared actions/configuration stay in Server; panels do not own input sessions |

Direct2D is the only renderer. The WebView2 small windows, the WebView2 settings host and
the `ui-html/` pages have been removed, together with the `appearance.ui_backend` and
`appearance.settings_ui_backend` configuration keys; existing values are dropped by the
template merge on upgrade.

The candidate worker resolves output text, auxiliary codes, source badges and
translations once into `CandidateViewItem`, and the native renderer consumes these
values as plain text. Clicks enter the existing `EnqueueCandidateUiAction` queue with the
focus owner checks and action policy. The ten-entry mouse range is distinct from the
1–9 keyboard shortcuts. New business behavior belongs in shared actions/session/view
models, not in a presenter.

## Settings window

`MetasequoiaImeSettings.exe` draws its window with msimeui in `src/settings/native/`.
The window speaks the message contract in `engine/contracts/webview/` (the name is
historical): it receives `configSnapshot` and the response messages as JSON and posts
`configUpdate` and the request messages through `HandleClientMessage` in
`settings_app.cpp`, so config paths are mapped in one place. Its image assets live in
`server/assets/settings/` and are installed to `<data dir>\assets\settings\`.

The pages were ported one to one from the former HTML/CSS settings page: each page
builder in `native_page_*.cpp` names the partial and module it came from, and the
controls in `native_settings_controls.*` name the CSS rule they mirror. Those sources
remain in git history under `ui-html/webview2/settings/ime-settings/src/`.

Sciter is not an active product backend. The msimeui library owns generic controls and
rendering, never Server configuration, pipe sessions, dictionary access, or global
input state.

## v2 skins

`schema_version = 2` skins (nine-slice panel images, freely anchored decorations, space
outside the panel) are rendered by `CandidatePresenter`'s native scene: the panel is drawn
by `msimeui::Card::SetBackgroundImageNineSlice`, decorations by the `CandidateSkinOverlay`
layer above the card, and `outer_insets_dip` becomes extra window space around the card.
Parts of v2 are still being completed on the native side; see the `TODO(v2 皮肤)` notes in
`candidate_presenter.cpp` and `candidate_skin_overlay.cpp`.

`anchor` and `offset_*_dip` follow one convention: positive values move right and down.
`content_insets_dip` is measured from the outer edge of the panel and is carried by the
card padding.
