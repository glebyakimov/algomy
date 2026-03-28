/**
 * @file A.cpp
 * @brief Wordle-style interactive solver for A-Worlde contest task.
 *
 * Guesses a hidden word from the dictionary in at most 6 attempts per round.
 * Strategy: minimax on worst-case remaining set size, with entropy as tie-breaker.
 * When candidate-only guesses give poor splits, uses informational words from
 * the full dictionary.
 */

 #include <cstdint>
 #include <cmath>
 #include <iostream>
 #include <string>
 #include <vector>

 #define MAX_LEN 7
 #define E_ALPH  26
 
 enum {
   MAX_ANSWERS  = 2100,   ///< Max number of possible answers (candidates).
   CACHE_SZ     = 262144, ///< Memoization cache size (power of 2).
   MAX_PATTERNS = 2187    ///< Max feedback patterns (3^7 for L<=7).
 };
 
 int dict_sz  ; ///< Total dictionary size (N).
 int n_rounds ; ///< Number of rounds (M).
 int word_len ; ///< Word length (L).
 int n_answers; ///< Number of candidates: min(dict_sz, MAX_ANSWERS).
 
 std::vector<std::string>   words   ;    ///< Full dictionary.
 std::vector<unsigned char> fb_cache;    ///< Precomputed feedback: fb_cache[g*n_answers+s].
 
 double ln2[MAX_ANSWERS + 1];            ///< Precomputed log2 for entropy.
 
 /** Memoization slot: caches best guess for a given set of possible words. */
 struct Memo {
   uint64_t key  ;      ///< Hash of the possible-words set.
   int      guess;      ///< Best guess index.
   bool     ok   ;      ///< Whether this slot is valid.
 } memo[CACHE_SZ];
 
 /**
  * @brief Computes feedback code between guess and secret.
  *
  * Encodes each position in base-3: 2 = exact match (#), 1 = misplaced (?), 0 = absent (-).
  * Position 0 is the least significant digit. Handles repeated letters: exact matches
  * are assigned first, then misplaced using remaining letter counts in the secret.
  *
  * @param g   Guessed word.
  * @param s   Secret (hidden) word.
  * @param len Word length.
  * @return Base-3 encoded feedback code.
  */
 static int encode_feedback(const std::string& g, const std::string& s, int len) {
   int code = 0, pow3 = 1;
   int cnt  [E_ALPH ] = {0};
   bool done[MAX_LEN] = {false};
 
   for (int i = 0; i < len; i++) cnt[s[i] - 'a']++;
   for (int i = 0; i < len; i++) {

     if (g[i] == s[i]) {
       code += 2 * pow3;

       cnt[g[i] - 'a']--;
       done[i] = true;
     }
     pow3 *= 3;
   }
   pow3 = 1;

   for (int i = 0; i < len; i++) {
     if (!done[i] && cnt[g[i] - 'a'] > 0) {

       code += pow3;
       cnt[g[i] - 'a']--;
     }

     pow3 *= 3;
   }
   return code;
 }
 
 /**
  * @brief Precomputes feedback for all (guess, secret) pairs.
  *
  * Fills fb_cache[g * n_answers + s] for g in [0, dict_sz), s in [0, n_answers).
  * Allows using any dictionary word as a guess (including info-words).
  */
 static void build_fb_cache() {
   fb_cache.resize((size_t)dict_sz * n_answers);

   for (int gi = 0; gi < dict_sz; gi++) {
     for (int si = 0; si < n_answers; si++)
     fb_cache[gi * n_answers + si] = (unsigned char)encode_feedback(words[gi], words[si], word_len);
   }
 }
 
 /**
  * @brief Rolling hash for a set of word indices.
  * @param idxs Sorted indices of currently possible words.
  * @return Hash value for cache lookup.
  */
 static uint64_t hash_state(const std::vector<int>& idxs) {
   uint64_t h = 0;
   for (int x : idxs) h = h * 0x9e3779b97f4a7c15ULL + (uint64_t)(x + 1);
   return h;
 }
 
 /**
  * @brief Score of a candidate guess for lexicographic comparison.
  */
 struct Score {
   int    worst_bucket; ///< Size of largest feedback group (smaller is better).
   double info;         ///< Information entropy (larger is better).
   int    word_idx;     ///< Word index in dictionary.
   bool   is_answer;    ///< True if guess is a possible answer (tie-breaker).
 
   /** Returns true if this score is strictly better than o. */
   bool beats(const Score& o) const {
     if (worst_bucket != o.worst_bucket) return worst_bucket < o.worst_bucket;
     
     if (fabs(info - o.info) > 1e-12) return info > o.info;
     return is_answer > o.is_answer;
   }
 };
 
 /**
  * @brief Evaluates a single guess against the current possible set.
  *
  * Groups possible secrets by their feedback pattern for this guess.
  * Computes max group size and entropy: H = log(n) - (1/n)*sum(c*log(c)).
  *
  * @param g     Guess word index.
  * @param rem   Indices of remaining possible words.
  * @param mask  Boolean mask: mask[i]==1 if word i is possible.
  * @param n     Number of possible words.
  * @return Score for this guess.
  */
 static Score rate_guess(int g,
                         const std::vector<int>& rem,
                         const std::vector<char>& mask,
                         int n) {
  int pat_freq[MAX_PATTERNS] = {0}; ///< Temporary: frequency of each pattern.
  int pat_ids [MAX_PATTERNS];       ///< Temporary: list of patterns used in current evaluation.
   int k = 0;
   const unsigned char* row = fb_cache.data() + (size_t)g * n_answers;
 
   for (int s : rem) {
     int c = row[s];
     if (pat_freq[c] == 0) pat_ids[k++] = c;
     pat_freq[c]++;
   }
 
   int mx     = 0;
   double acc = 0;

   for (int i = 0; i < k; i++) {
     int c = pat_freq[pat_ids[i]];

     if (c > mx) mx = c;

     acc += c * ln2[c];
     pat_freq[pat_ids[i]] = 0;
   }
 
   Score  sc;
          sc.worst_bucket = mx;
          sc.info         = ln2[n] - acc / n;
          sc.word_idx     =  g;
          sc.is_answer    = (g < n_answers && mask[g]);
   return sc;
 }
 
 /**
  * @brief Selects the best guess for the current set of possible words.
  *
  * Phase 1: evaluate only possible answers. Phase 2: if best split is poor
  * (worst bucket > 2), also try informational words from the full dictionary.
  * Uses memoization keyed by hash of the possible set.
  *
  * @param mask   Boolean mask of possible words.
  * @param n_rem  Number of possible words.
  * @return Index of the best guess.
  */
 static int pick_guess(const std::vector<char>& mask, int n_rem) {
   if (n_rem == 1) {
     for (int i = 0; i < n_answers; i++)
       if (mask[i]) return i;
   }
 
   std::vector<int> rem;
   rem.reserve   (n_rem);

   for (int i = 0; i < n_answers; i++)
     if (mask[i]) rem.push_back(i);
 
   uint64_t h = hash_state(rem);
   int slot = h & (CACHE_SZ - 1);

   if (memo[slot].ok && memo[slot].key == h) return memo[slot].guess;
 
   Score best = {1000000000, -1.0, -1, false};
 
   // Phase 1: candidates only.
   for (int g : rem) {
     Score s = rate_guess(g, rem, mask, n_rem);

     if (best.word_idx < 0 || s.beats(best)) best = s;
     if (best.worst_bucket == 1) break;
   }
 
   // Phase 2: informational words when candidates give poor split.
   if (best.worst_bucket > 2) {
     for (int g = 0; g < dict_sz; g++) {

       if (g < n_answers && mask[g]) continue;
       Score s = rate_guess(g, rem, mask, n_rem);

       if (s.beats(best)) best = s;
       if (best.worst_bucket == 1) break;
     }
   }
 
   memo[slot] = {h, best.word_idx, true};
   return best.word_idx;
 }
 
 /**
  * @brief Main: reads input, precomputes, plays M interactive rounds.
  */
 int main() {
   std::cin >>  dict_sz >> n_rounds >> word_len;
   words.resize(dict_sz);

   for (int i = 0; i < dict_sz; i++) std::cin >> words[i];
 
   n_answers = std::min(dict_sz, (int)MAX_ANSWERS);
   for (int i = 1; i <= MAX_ANSWERS; i++) ln2[i] = std::log2(i);
 
   build_fb_cache();
   for (int i = 0; i < CACHE_SZ; i++) memo[i].ok = false;
 
   std::vector<char> mask(n_answers, 1);
   int first = pick_guess(mask, n_answers);
 
   for (int r = 0; r < n_rounds; r++) {
     mask.assign(n_answers, 1);
     int n = n_answers;
 
     for (;;) {
       int g = (n == n_answers) ? first : pick_guess(mask, n);
       std::cout << words[g] << '\n';
 
       std::string resp;
       if (!(std::cin >> resp)) return 0;
 
       bool done = true;
       for (char c : resp)
         if (c != '#') { done = false; break; }
       if (done) break;
 
       // Encode judge response to pattern id (same convention as encode_feedback).
       int code = 0, pow3 = 1;
       for (int i = 0; i < word_len; i++) {
         code += (resp[i] == '#' ? 2 : resp[i] == '?' ? 1 : 0) * pow3;
         pow3 *= 3;
       }
 
       const unsigned char* row = fb_cache.data() + (size_t)g * n_answers;
       int nn = 0;
       for (int s = 0; s < n_answers; s++) {
         if (mask[s] && row[s] == code) {
             mask[s] = 1;
            nn++;
            
         } else
           mask[s] = 0;
       }
       n = nn;
     }
   }
   return 0;
 }