#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "ya_communicate/module/survey.h"

namespace ya::module {

class SurveyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use unique port to avoid conflicts
    static int port = 5555;
    address_ = "tcp://127.0.0.1:" + std::to_string(port++);

    // Create initiator first to bind to address
    initiator_ =
        std::make_unique<Survey>(ya::module::Survey::ROLE::INITIATOR, address_);
    // Small delay to ensure initiator is listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Create two voters
    voter1_ =
        std::make_unique<Survey>(ya::module::Survey::ROLE::VOTER, address_);
    voter2_ =
        std::make_unique<Survey>(ya::module::Survey::ROLE::VOTER, address_);
  }

  void TearDown() override {
    voter1_.reset();
    voter2_.reset();
    initiator_.reset();
  }

  std::string address_;
  std::unique_ptr<Survey> initiator_;
  std::unique_ptr<Survey> voter1_;
  std::unique_ptr<Survey> voter2_;
};

TEST_F(SurveyTest, SendAndReceiveSurvey) {
  std::string survey_message = "What is your vote?";
  std::thread voter_thread([this, &survey_message]() {
    std::string received = voter1_->receive_survey();
    EXPECT_EQ(received, survey_message);
  });

  initiator_->send_survey(survey_message);
  voter_thread.join();
}

TEST_F(SurveyTest, CollectResponses) {
  std::string survey_message = "Vote yes or no";
  std::vector<std::string> responses = {"Yes", "No"};

  // Start voter threads to receive survey and respond
  std::thread voter1_thread([this, &survey_message, &responses]() {
    std::string received = voter1_->receive_survey();
    EXPECT_EQ(received, survey_message);
    voter1_->respond(responses[0]);
  });
  std::thread voter2_thread([this, &survey_message, &responses]() {
    std::string received = voter2_->receive_survey();
    EXPECT_EQ(received, survey_message);
    voter2_->respond(responses[1]);
  });

  initiator_->send_survey(survey_message);
  auto collected_responses = initiator_->collect_responses();
  voter1_thread.join();
  voter2_thread.join();

  ASSERT_EQ(collected_responses.size(), responses.size());
  // Responses may arrive in any order
  std::vector<std::string> expected = responses;
  std::sort(collected_responses.begin(), collected_responses.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(collected_responses, expected);
}

TEST_F(SurveyTest, VoterCannotSendSurvey) {
  EXPECT_THROW(voter1_->send_survey("Should fail"), std::runtime_error);
}

TEST_F(SurveyTest, InitiatorCannotRespond) {
  EXPECT_THROW(initiator_->respond("Should fail"), std::runtime_error);
}

TEST_F(SurveyTest, InitiatorCannotReceiveSurvey) {
  EXPECT_THROW(initiator_->receive_survey(), std::runtime_error);
}

TEST_F(SurveyTest, VoterCannotCollectResponses) {
  EXPECT_THROW(voter1_->collect_responses(), std::runtime_error);
}

TEST(SurveyInvalidAddressTest, InvalidAddressThrows) {
  EXPECT_THROW(Survey(ya::module::Survey::ROLE::INITIATOR, "invalid://address"),
               std::runtime_error);
  EXPECT_THROW(Survey(ya::module::Survey::ROLE::VOTER, "tcp://invalid:port"),
               std::runtime_error);
}

TEST(SurveyNoVotersTest, CollectResponsesWithNoVoters) {
  std::string address = "tcp://127.0.0.1:5567";
  Survey initiator(ya::module::Survey::ROLE::INITIATOR, address);
  initiator.send_survey("No voters");
  auto responses = initiator.collect_responses();
  EXPECT_TRUE(responses.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

}  // namespace ya::module
