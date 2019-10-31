local wasmer = require "wasmer"

local code = io.open("simple.wasm", "rb"):read("*a")

local instance = wasmer.instantiate(code)
local exports = instance:exports()
print(exports.sum(1, 4))
for k,v in pairs(exports) do
  print(k,v)
end
