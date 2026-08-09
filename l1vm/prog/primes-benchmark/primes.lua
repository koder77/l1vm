local ffi = require("ffi")
local bit = require("bit")
local ffi = require("ffi")

function sieve_fast(n)
    local is_prime = ffi.new("uint8_t[?]", n + 1)
    ffi.fill(is_prime, n + 1, 1)

    is_prime[0], is_prime[1] = 0, 0

    for p = 2, math.sqrt(n) do -- Optimierung: nur bis Wurzel n
        if is_prime[p] == 1 then
            for i = p * p, n, p do
                is_prime[i] = 0
            end
        end
    end
    -- Wir geben das Array zurück, damit wir es draußen nutzen können
    return is_prime
end

local N = 100000000
local start = os.clock()
local my_primes_array = sieve_fast(N) -- Array hier auffangen
local duration = os.clock() - start

print(string.format("Search done in %.4f s", duration))

for i = 2, N  do
    if my_primes_array[i] == 1 then
        io.write(i .. " ")
        io.write("\n")
    end
end
print()
