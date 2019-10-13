local wasmer = require "wasmer"

local code = io.open("simple.wasm", "rb"):read("*a")

local instance = wasmer.instantiate(code)
print(instance:exports().sum(1, 4))
