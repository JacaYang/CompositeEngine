import { all, fork, put, takeEvery } from 'redux-saga/effects';
import { sendToggleAnimationRequest } from "../ipc";
import { AnimationMutationTypes, toggleAnimationFailure, toggleAnimationSuccess } from "./actions";

const {
  TOGGLE_ANIMATION_REQUEST
} = AnimationMutationTypes;

function* requestToggleAnimationStateAsync() {
  try {
    const animationState = yield sendToggleAnimationRequest();
    yield put(toggleAnimationSuccess(animationState));
  } catch (e) {
    yield put(toggleAnimationFailure());
  }
}

function* watchToggleAnimation() {
  yield takeEvery(TOGGLE_ANIMATION_REQUEST, requestToggleAnimationStateAsync);
};

export default function* root() {
  yield all([
    fork(watchToggleAnimation)
  ]);
};
