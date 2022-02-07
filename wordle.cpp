#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <mutex>
#include <thread>
#include <algorithm>

enum class Result
{
	Grey,
	Yellow,
	Green
};

struct State
{
	std::string    knownLetters = "00000";
	std::set<char> knownPresentLetters;
	std::set<char> knownAbsentLetters;
};

void updateStateWithResult(State& state, const std::string& guess, const std::vector<Result>& result)
{
	for (char i = 0; i < 5; i++)
	{
		if (result[i] == Result::Grey)
		{
			state.knownAbsentLetters.insert(guess[i]);
		}
	}

	for (char i = 0; i < 5; i++)
	{
		if (result[i] == Result::Yellow)
		{
			state.knownPresentLetters.insert(guess[i]);
			state.knownAbsentLetters.erase(guess[i]);
		}
	}
	
	for (char i = 0; i < 5; i++)
	{
		if (result[i] == Result::Green)
		{
			state.knownLetters[i] = guess[i];
			state.knownAbsentLetters.erase(guess[i]);
		}
	}
}

bool isWordPossibleGivenState(const State& state, const std::string& word)
{
	for (char i = 0; i < 5; i++)
	{
		if (state.knownLetters[i] != '0' && state.knownLetters[i] != word[i])
		{
			return false;
		}
	}
	
	for (const auto& l : word)
	{
		if (state.knownAbsentLetters.find(l) != state.knownAbsentLetters.end())
		{
			return false;
		}
	}
	
	for (const auto& l : state.knownPresentLetters)
	{
		if (word.find(l) == std::string::npos)
		{
			return false;
		}
	}
	
	return true;
}

std::vector<Result> getResultGivenAnswer(const std::string& guess, const std::string& answer)
{
	auto resultForGuess = std::vector<Result>(5, Result::Grey);
	
	for (char i = 0; i < 5; i++)
	{
		if (guess[i] == answer[i])
		{
			resultForGuess[i] = Result::Green;
		}
		else if (answer.find(guess[i]) != std::string::npos)
		{
			resultForGuess[i] = Result::Yellow;
		}
	}
	
	return resultForGuess;
};

float getExpectedValueForGuess(const std::string& guess, const State& state, const std::vector<std::string>& remainingWords)
{
	std::map<std::vector<Result>, unsigned int> resultFrequencyMap;
	
	for (const auto& possibleAnswer : remainingWords)
	{
		resultFrequencyMap[getResultGivenAnswer(guess, possibleAnswer)]++;
	}
	
	const float numRemainingWords = (float) remainingWords.size();
	float expectedValue = 0;
	int wordsPossibleGivenResult = 0;
	auto stateGivenResult = state;
	
	for (const auto& [result, frequency] : resultFrequencyMap)
	{
		stateGivenResult = state;
		updateStateWithResult(stateGivenResult, guess, result);
		
		wordsPossibleGivenResult = 0;
		
		for (const auto& word : remainingWords)
		{
			if (isWordPossibleGivenState(stateGivenResult, word))
			{
				wordsPossibleGivenResult++;
			}
		}

		expectedValue += (frequency / numRemainingWords * std::log2(numRemainingWords/ (float) wordsPossibleGivenResult));
	}

	return expectedValue;
}

void guess(unsigned int numThreads, const State& state, const std::vector<std::string>& allWords, const std::vector<std::string>& remainingWords)
{
	std::cout << "\n" << std::to_string(remainingWords.size()) << " words remaining";
	
	if (remainingWords.size() <= 20)
	{
		std::cout << ":\n";
		
		for (const auto& word: remainingWords)
		{
			std::cout << "\n" << word;
		}
	}
	
	if (remainingWords.size() <= 2)
	{
		std::cout << "\n" << std::endl;
		std::string unused;
		while(true)
		{
			std::cin >> unused;
		}
	}
	
	
	std::cout << "\n\nWorking...\n" << std::endl;
	
	std::vector<std::pair<std::string, float>> expectedValues;
	expectedValues.reserve(remainingWords.size());
	std::mutex mutex;

	const auto parallelFunc = [&numThreads, &expectedValues, &mutex, &state, &allWords, &remainingWords](const unsigned int threadIndex) {
		for (unsigned int i = 0; i < allWords.size(); i++)
		{
			if (i % numThreads != threadIndex)
			{
				continue;
			}
			
			const auto expectedValue = getExpectedValueForGuess(allWords[i], state, remainingWords);
		
			std::lock_guard<std::mutex> lock(mutex);
			expectedValues.emplace_back(std::make_pair<>(allWords[i], expectedValue));
		}
	};

	std::vector<std::thread> threads;
	threads.reserve(numThreads);

	for (unsigned int i = 0; i < numThreads; i++)
	{
		threads.emplace_back(std::thread(parallelFunc, i));
	}
	
	for (auto& thread : threads)
	{
		thread.join();
	}
	
	std::sort(expectedValues.begin(), expectedValues.end(), [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
	
	if (expectedValues.size() > 10)
	{
		expectedValues.resize(10);
	}
	
	std::cout << "Best guesses to reduce the number of words remaining:\n\n";
	
	for (const auto& pair : expectedValues)
	{
		std::cout << pair.first << " \t" << std::to_string(pair.second) << "\n";
	}
}

std::vector<std::string> loadWords()
{
	std::vector<std::string> words;
	std::string word;
	
	std::ifstream infile("words.txt");
	
	while (infile >> word)
	{
		words.emplace_back(word);
	}
	
	return words;
};

int main()
{
	auto numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0)
	{
		numThreads = 8;
	}
	
	const auto allWords = loadWords();
	auto remainingWords = allWords;
	
	State state;
	
	bool skipFirst = true;
	
	while(true)
	{
		if (skipFirst)
		{
			std::cout << "\nBest guess: \"serai\"" << std::endl; // Always the best first guess
			skipFirst = false;
		}
		else
		{
			guess(numThreads, state, allWords, remainingWords);
		}
		
		std::string userInput;
		
		while(true)
		{
			std::cout << "\nEnter guessed word:" << std::endl;
			std::cin >> userInput;
			
			if (std::find(allWords.begin(), allWords.end(), userInput) != allWords.end())
			{
				break;
			}
			
			std::cout << "Error!";
		}
		
		const auto guessedWord = userInput;
		
		while (true)
		{
			std::cout << "\nEnter result (5 letters, x=grey, y=yellow, g=green):" << std::endl;
			std::cin >> userInput;
			
			bool error = false;
			
			if (userInput.size() != 5)
			{
				error = true;
			}
			
			auto result = std::vector<Result>(5);
			
			for (char i = 0; i < 5; i++)
			{
				if (userInput[i] == 'y')
				{
					result[i] = Result::Yellow;
				}
				else if (userInput[i] == 'g')
				{
					result[i] = Result::Green;
				}
				else if (userInput[i] != 'x')
				{
					error = true;
				}
			}
			
			if (error)
			{
				std::cout << "Error!";
			}
			else
			{
				updateStateWithResult(state, guessedWord, result);
				break;
			}
		}
		
		std::vector<std::string> newRemainingWords;
		
		std::copy_if(remainingWords.begin(), remainingWords.end(), std::back_inserter(newRemainingWords), [&state](const auto& word) {
			return isWordPossibleGivenState(state, word);
		});
		
		std::swap(remainingWords, newRemainingWords);
	}
	
	return 0;
}
