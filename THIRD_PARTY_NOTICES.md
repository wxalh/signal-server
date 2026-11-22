# Third-Party Notices

The release package includes the following vendored components. Their
copyright and license terms remain those in the referenced files:

- Asio: Boost Software License 1.0, `third_party/asio/asio/LICENSE_1_0.txt`.
- WebSocket++: BSD-style license, `third_party/websocketpp/COPYING`.
- nlohmann/json: MIT license, `third_party/json/LICENSE.MIT`.
- spdlog (including bundled fmt): MIT license, `third_party/spdlog/LICENSE`.

The exact vendored source revisions are pinned by `.gitmodules` and the
repository checkout used for the build. A distributor must preserve these
notices and verify any submodule changes before publishing a package.
