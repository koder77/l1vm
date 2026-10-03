/*
 * This file provider_llamacpp.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
 *
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * brackets-llm - the llama.cpp backend
 *
 * llama-server speaks the same OpenAI-compatible protocol as everything else,
 * so this backend reuses the shared REST core (provider_openai.c) and only
 * keeps the two llama.cpp specific details that brackets-llm has always sent:
 *
 *   - "chat_template_kwargs": {"enable_thinking": false}
 *     Qwen3-style thinking models otherwise spend the whole token budget on
 *     reasoning instead of writing Brackets code.
 *   - no "Authorization" header (llama-server has no auth and only logs
 *     unknown headers) and never SSE streaming, i.e. the historical
 *     request/response behaviour.
 */

#include "provider.h"

#include <stdlib.h>

static const LlmRestFlavor llamacpp_flavor = {
    LLM_BACKEND_LLAMACPP,
    0,      /* send_auth: llama-server needs no token */
    0,      /* allow_stream: buffered answers only */
    1       /* add_thinking_ctl: enable_thinking = false */
};

LlmProvider *llm_llamacpp_provider_new(LlmConfig *cfg)
{
    return llm_rest_provider_new(cfg, &llamacpp_flavor);
}
