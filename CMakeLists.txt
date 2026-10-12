cmake_minimum_required(VERSION 3.16)
project(jlpt_trainer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_executable(jlpt-trainer
  src/main.cpp
  src/App.cpp
  src/AppContext.cpp
  src/AudioPlayer.cpp
  src/Config.cpp
  src/DeepSeekClient.cpp
  src/Furigana.cpp
  src/Json.cpp
  src/MistakeTracker.cpp
  src/ProgressTracker.cpp
  src/Question.cpp
  src/QuestionBank.cpp
  src/QuizSession.cpp
  src/SRSManager.cpp
  src/Terminal.cpp
  src/TtsEngine.cpp
  src/Util.cpp
  src/modules/GrammarModule.cpp
  src/modules/ListeningModule.cpp
  src/modules/MistakesModule.cpp
  src/modules/ProgressModule.cpp
  src/modules/SRSModule.cpp
  src/modules/VocabularyModule.cpp
)

target_include_directories(jlpt-trainer PRIVATE include)
# Lets the binary find data/ even when started from another directory.
target_compile_definitions(jlpt-trainer PRIVATE JLPT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(jlpt-trainer PRIVATE Threads::Threads)
target_compile_options(jlpt-trainer PRIVATE -Wall -Wextra -Wpedantic)
