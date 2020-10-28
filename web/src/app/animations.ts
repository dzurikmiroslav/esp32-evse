import {
    animate,
    animateChild,
    AUTO_STYLE,
    group,
    query,
    state,
    style,
    transition,
    trigger
} from '@angular/animations';

export const fadeInOutAnimation = trigger(('fadeInOut'), [
    state('0, void', style({
        opacity: '0',
        display: 'none',
    })),
    state('1', style({
        opacity: AUTO_STYLE,
        display: AUTO_STYLE,
    })),
    transition('* => 1', [
        group([
            query('@*', animateChild(), {optional: true}),
            animate('150ms 0ms ease-in'),
        ]),
    ]),
    transition('* => 0', [
        group([
            query('@*', animateChild(), {optional: true}),
            animate('150ms 0ms ease-out'),
        ]),
    ]),
]);
