import { default as React } from 'react';
import ReactDOM from 'react-dom';
import { Provider as ReduxProvider } from 'react-redux';
import { applyMiddleware, createStore } from 'redux';
import { composeWithDevTools } from 'remote-redux-devtools';
import logger from 'redux-logger';
import createSagaMiddleware from 'redux-saga';
import App from './App';
import './index.css';
import reducer from './redux/reducers/index';
import { updatePauseState, updateAnimationState } from './redux/actions';
import rootSaga from './redux/sagas';
import * as serviceWorker from './serviceWorker';
import { subscribeToPauseState, subscribeToAnimationState } from './ipc';

// create the saga middleware
const sagaMiddleware = createSagaMiddleware()

// create the redux store
const enhancers = composeWithDevTools(
  applyMiddleware(
    sagaMiddleware,
    // logger
  ),
);
const store = createStore(reducer, enhancers);

sagaMiddleware.run(rootSaga);

subscribeToPauseState((state) => {
  store.dispatch(updatePauseState(state));
});
subscribeToAnimationState((state) => {
  store.dispatch(updateAnimationState(state));
});

ReactDOM.render(
  <ReduxProvider store={store}>
    <App />
  </ReduxProvider>,
  document.getElementById('root'));

// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: http://bit.ly/CRA-PWA
serviceWorker.unregister();
